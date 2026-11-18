// A 2D screen at the picture's full height (RT64's Aspect Ratio Expand): the title, the menus and the screens between
// them fill the window's height, or its width where the window is no wider than 4:3, their shape kept, centred, the
// rest black.
//
// The game draws every screen but a race in a frame of its own, 12, 8 to 308, 232: the 296x224 a TV's edge left in
// sight, black round it. RT64 keeps such a screen at 4:3 in the middle of a wider picture, so it stood in black above
// and below as well as beside it, and stretches a rectangle as wide as its scissor (a backdrop) across the window.
//
// Here each framed screen (widescreen.c's func_80069698 says which, and calls Screen_Fill with the commands it and its
// transition wrote) is scaled about its picture's middle until the picture's lines fill the 240, or in a window no
// wider than 4:3 until the frame's 296 columns fill the 320, by 320/296, its middle at the picture's. A picture is the
// frame's 224 lines, by 240/224, but for the title's art (the title, the main menu, options and course select), whose
// first 16 lines are black: there it is lines 16 to 232, by 240/216, and the frame's sides go 4 columns past the
// picture's (Screen_GetTop). In those commands and the lists they call:
// - each texture rectangle is scaled, its edges on whole pixels (Screen_ScaleRect says why), and clipped to the 4:3
//   frame, its texture start moved to match, and drawn point sampled through a list that keeps it at its shape
//   (gEXSetRectAspect(G_EX_ASPECT_ADJUST)). The game filters them bilinearly, which at 1:1 reads each texel as it is,
//   but scaled it blurs a strip of a backdrop into the next and stops at its edges (a strip's texture is only its own
//   rows), so the strips would show as bands. Each fill rectangle (a menu's panel or backdrop, a transition's black)
//   is scaled the same way and kept at its shape too, in two halves where it spans the screen's middle: RT64 places a
//   fill-mode one by its own rule, stretched across the window if it is as wide as its scissor, whatever the aspect
//   set (machine select's backdrop is 224 of them, a line each);
// - each scissor is scaled the same way and clipped to the frame, a quarter pixel inside the frame's sides where the
//   game's was inside them: RT64 widens a model to the window when its scissor spans the whole width the frame drew
//   in, and a menu's models (the machines) are kept at the screen's shape. Nothing reaches past the frame's sides:
//   RT64 takes the widest scissor a frame draws under as the screen;
// - each viewport is a copy scaled the same way, so a model on a menu grows with it.
// The frame's own commands are changed where they are, since the game writes them anew each frame. A list they call is
// the game's data, so one that holds anything to scale is copied, the copy scaled, and called in its place; the game's
// segment table (gSegments, and any segment the commands set) says where a segmented one is. The copies, the
// rectangles' lists and the viewports are this file's, in an arena a frame, used in turn: the renderer may read a
// list a frame or two after the game sends it.

#include "patches.h"
#include "fzx_game.h"
#include "screen.h"
#include "rt64_extended_gbi.h"

extern u32 gSegments[16];

// Gfx an arena holds, and the arenas used in turn, one a frame.
#define SCREEN_ARENA_SIZE 0x8000
#define SCREEN_ARENA_COUNT 4
// How deep lists call lists, and the commands one list may have, at most.
#define SCREEN_MAX_DEPTH 8
#define SCREEN_MAX_COMMANDS 0x4000

#define SCREEN_CENTRE_X (SCREEN_WIDTH / 2.0f)
#define SCREEN_CENTRE_Y (SCREEN_HEIGHT / 2.0f)
// The frame, the game's 12, 8 to 308, 232.
#define SCREEN_FRAME_WIDTH 296.0f
#define SCREEN_FRAME_BOTTOM 232.0f

// G_MOVEMEM of a viewport (gSPViewport), and G_SETOTHERMODE_H of the cycle type (gDPSetCycleType).
#define SCREEN_MOVEMEM_VIEWPORT 0xDC080008
#define SCREEN_SET_CYCLE_TYPE 0xE3000A01

static Gfx sScreenArenas[SCREEN_ARENA_COUNT][SCREEN_ARENA_SIZE];
static s32 sScreenArena = 0;
static s32 sScreenArenaUsed = 0;
static u32 sScreenSegments[16];
static u32 sScreenCycleType = G_CYC_1CYCLE;
static f32 sScreenFill = 1.0f;
static f32 sScreenCentreY = SCREEN_CENTRE_Y;

// The first line of a screen's picture: the title's art (aTitleBackgroundMainTex, Comic and Falcon, 304 or 320 by 240
// and drawn from the screen's top) is black above its 16th, which the frame's 8 leaves in sight.
static f32 Screen_GetTop(s32 mode) {
    switch (mode) {
        case GAMEMODE_TITLE:
        case GAMEMODE_MAIN_MENU:
        case GAMEMODE_COURSE_SELECT:
        case GAMEMODE_GP_RACE_NEXT_COURSE:
        case GAMEMODE_RECORDS_COURSE_SELECT:
        case GAMEMODE_OPTIONS_MENU:
            return 16.0f;
        default:
            return 8.0f;
    }
}

f32 Screen_GetFill(s32 mode, f32* centreY) {
    f32 aspect = recomp_get_aspect_ratio(0.0f);
    f32 top = Screen_GetTop(mode);

    *centreY = (top + SCREEN_FRAME_BOTTOM) / 2.0f;
    // With Aspect Ratio Original, none.
    if (aspect <= 0.0f) {
        return 0.0f;
    }
    if (aspect > 4.0f / 3.0f) {
        return SCREEN_HEIGHT / (SCREEN_FRAME_BOTTOM - top);
    }
    return SCREEN_WIDTH / SCREEN_FRAME_WIDTH;
}

static Gfx* Screen_Alloc(s32 count) {
    Gfx* gfx;

    if (sScreenArenaUsed + count > SCREEN_ARENA_SIZE) {
        return NULL;
    }
    gfx = &sScreenArenas[sScreenArena][sScreenArenaUsed];
    sScreenArenaUsed += count;
    return gfx;
}

// The list at a segmented address, as the RSP finds it, or NULL past RAM.
static Gfx* Screen_Resolve(u32 addr) {
    u32 phys = (sScreenSegments[(addr >> 24) & 0xF] + (addr & 0xFFFFFF)) & 0x1FFFFFFF;

    if ((phys >= 0x01000000) || (phys & 7)) {
        return NULL;
    }
    return (Gfx*) (phys | 0x80000000);
}

static f32 Screen_ScaleX(f32 x) {
    return SCREEN_CENTRE_X + (x - SCREEN_CENTRE_X) * sScreenFill;
}

static f32 Screen_ScaleY(f32 y) {
    return SCREEN_CENTRE_Y + (y - sScreenCentreY) * sScreenFill;
}

static s32 Screen_Round(f32 v) {
    return (v >= 0.0f) ? (s32) (v + 0.5f) : -(s32) (0.5f - v);
}

// The first whole pixel at or past v, inside 0 to limit.
static s32 Screen_Edge(f32 v, f32 limit) {
    s32 i;

    v = (v < 0.0f) ? 0.0f : ((v > limit) ? limit : v);
    v -= 0.001f;
    i = (s32) v;
    return (i < v) ? (i + 1) : i;
}

// The rectangle's corners in quarter pixels, from a texture or fill rectangle's command and its second word.
#define RECT_ULX(w1) ((((w1) >> 12) & 0xFFF) / 4.0f)
#define RECT_ULY(w1) (((w1) & 0xFFF) / 4.0f)
#define RECT_LRX(w0) ((((w0) >> 12) & 0xFFF) / 4.0f)
#define RECT_LRY(w0) (((w0) & 0xFFF) / 4.0f)

// The rectangle's corners scaled and clipped to the 4:3 frame, false if nothing of it is left. A rectangle's texture
// starts at the first whole pixel it covers, so one starting between two would draw its rows up to a line off: each
// edge is the first whole pixel at or past it, the same for two rectangles that meet there, so they still meet, and
// every pixel one covers reads its own rows of the texture. In copy and fill modes its lower right corner is the last
// pixel it covers, not the one past it.
static bool Screen_ScaleCorners(u32 w0, u32 w1, bool inclusive, s32* corners, f32* x0, f32* y0) {
    f32 lrx = RECT_LRX(w0) + (inclusive ? 1.0f : 0.0f);
    f32 lry = RECT_LRY(w0) + (inclusive ? 1.0f : 0.0f);

    *x0 = Screen_ScaleX(RECT_ULX(w1));
    *y0 = Screen_ScaleY(RECT_ULY(w1));
    corners[0] = Screen_Edge(*x0, SCREEN_WIDTH);
    corners[1] = Screen_Edge(*y0, SCREEN_HEIGHT);
    corners[2] = Screen_Edge(Screen_ScaleX(lrx), SCREEN_WIDTH);
    corners[3] = Screen_Edge(Screen_ScaleY(lry), SCREEN_HEIGHT);
    if ((corners[2] <= corners[0]) || (corners[3] <= corners[1])) {
        return false;
    }
    if (inclusive) {
        corners[2]--;
        corners[3]--;
    }
    return true;
}

// The texture rectangle at rect (its three commands) scaled, written at out, its texture start moved to the first
// whole pixel it covers.
static bool Screen_ScaleRect(Gfx* rect, Gfx* out, bool inclusive) {
    u32 w0 = rect[0].words.w0;
    u32 w1 = rect[0].words.w1;
    u32 st = rect[1].words.w1;
    u32 d = rect[2].words.w1;
    f32 s = (s16) (st >> 16) / 32.0f;
    f32 t = (s16) (st & 0xFFFF) / 32.0f;
    f32 dsdx = (s16) (d >> 16) / 1024.0f / sScreenFill;
    f32 dtdy = (s16) (d & 0xFFFF) / 1024.0f / sScreenFill;
    s32 c[4];
    f32 x0;
    f32 y0;

    if (!Screen_ScaleCorners(w0, w1, inclusive, c, &x0, &y0)) {
        return false;
    }
    s += (c[0] - x0) * dsdx;
    t += (c[1] - y0) * dtdy;
    out[0].words.w0 = (w0 & 0xFF000000) | ((u32) (c[2] * 4) << 12) | (u32) (c[3] * 4);
    out[0].words.w1 = (w1 & 0xFF000000) | ((u32) (c[0] * 4) << 12) | (u32) (c[1] * 4);
    out[1].words.w0 = rect[1].words.w0;
    out[1].words.w1 = ((u32) Screen_Round(s * 32.0f) << 16) | ((u32) Screen_Round(t * 32.0f) & 0xFFFF);
    out[2].words.w0 = rect[2].words.w0;
    out[2].words.w1 =
        ((u32) Screen_Round(dsdx * 1024.0f) << 16) | ((u32) Screen_Round(dtdy * 1024.0f) & 0xFFFF);
    return true;
}

// The texture rectangle at rect drawn scaled, point sampled and at its shape by a list it calls in its place, its
// other two commands made no-ops. One scaled to nothing calls a list that draws nothing.
static void Screen_ReplaceRect(Gfx* rect, bool inclusive) {
    Gfx* list = Screen_Alloc(9);
    Gfx* gfx = list;

    if (list == NULL) {
        return;
    }
    gEXPushOtherMode(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_POINT);
    gEXSetRectAspect(gfx++, G_EX_ASPECT_ADJUST);
    if (Screen_ScaleRect(rect, gfx, inclusive)) {
        gfx += 3;
    }
    gEXSetRectAspect(gfx++, G_EX_ASPECT_AUTO);
    gEXPopOtherMode(gfx++);
    gSPEndDisplayList(gfx++);
    gSPDisplayList(&rect[0], list);
    gSPNoOp(&rect[1]);
    gSPNoOp(&rect[2]);
}

// A fill rectangle's command from its corners, the lower right one as the cycle type takes it.
static void Screen_SetFill(Gfx* gfx, u32 w0, s32 ulx, s32 uly, s32 lrx, s32 lry) {
    gfx->words.w0 = (w0 & 0xFF000000) | ((u32) (lrx * 4) << 12) | (u32) (lry * 4);
    gfx->words.w1 = ((u32) (ulx * 4) << 12) | (u32) (uly * 4);
}

// The fill rectangle at rect drawn scaled and at its shape by a list it calls in its place: in two halves where it
// spans the screen's middle, so that neither is as wide as a scissor (the file's header says why), meeting there.
static void Screen_ReplaceFill(Gfx* rect, bool inclusive) {
    Gfx* list = Screen_Alloc(5);
    Gfx* gfx = list;
    u32 w0 = rect->words.w0;
    s32 middle = (s32) SCREEN_CENTRE_X;
    s32 c[4];
    f32 x0;
    f32 y0;

    if (list == NULL) {
        return;
    }
    gEXSetRectAspect(gfx++, G_EX_ASPECT_ADJUST);
    if (Screen_ScaleCorners(w0, rect->words.w1, inclusive, c, &x0, &y0)) {
        // c[2] is the last column with inclusive, the one past it without.
        if ((c[0] < middle) && (c[2] >= middle + (inclusive ? 0 : 1))) {
            Screen_SetFill(gfx++, w0, c[0], c[1], inclusive ? (middle - 1) : middle, c[3]);
            Screen_SetFill(gfx++, w0, middle, c[1], c[2], c[3]);
        } else {
            Screen_SetFill(gfx++, w0, c[0], c[1], c[2], c[3]);
        }
    }
    gEXSetRectAspect(gfx++, G_EX_ASPECT_AUTO);
    gSPEndDisplayList(gfx++);
    gSPDisplayList(rect, list);
}

// The scissor scaled and clipped to the frame, in place. One the game set inside the frame's sides stays a quarter
// pixel inside them: RT64 widens a model to the window when its scissor spans the whole width the frame drew in.
static void Screen_ScaleScissor(Gfx* gfx) {
    u32 w0 = gfx->words.w0;
    u32 w1 = gfx->words.w1;
    f32 ulx = RECT_ULX(w0);
    f32 uly = RECT_ULY(w0);
    f32 lrx = RECT_LRX(w1);
    f32 lry = RECT_LRY(w1);
    u32 newUlx = (u32) Screen_Edge(Screen_ScaleX(ulx), SCREEN_WIDTH) * 4;
    u32 newUly = (u32) Screen_Edge(Screen_ScaleY(uly), SCREEN_HEIGHT) * 4;
    u32 newLrx = (u32) Screen_Edge(Screen_ScaleX(lrx), SCREEN_WIDTH) * 4;
    u32 newLry = (u32) Screen_Edge(Screen_ScaleY(lry), SCREEN_HEIGHT) * 4;

    if ((ulx > 0.0f) && (newUlx < 1)) {
        newUlx = 1;
    }
    if ((lrx < SCREEN_WIDTH) && (newLrx > SCREEN_WIDTH * 4 - 1)) {
        newLrx = SCREEN_WIDTH * 4 - 1;
    }
    gfx->words.w0 = (w0 & 0xFF000000) | (newUlx << 12) | newUly;
    gfx->words.w1 = (w1 & 0xFF000000) | (newLrx << 12) | newLry;
}

// The viewport the G_MOVEMEM loads, copied, scaled about the screen's middle, and loaded from the copy. A viewport is
// four shorts of scale and four of translation, in quarter pixels.
static void Screen_ScaleViewport(Gfx* gfx) {
    Vp* vp = (Vp*) Screen_Resolve(gfx->words.w1);
    Vp* copy;

    if (vp == NULL) {
        return;
    }
    copy = (Vp*) Screen_Alloc(2);
    if (copy == NULL) {
        return;
    }
    *copy = *vp;
    copy->vp.vscale[0] = Screen_Round(vp->vp.vscale[0] * sScreenFill);
    copy->vp.vscale[1] = Screen_Round(vp->vp.vscale[1] * sScreenFill);
    copy->vp.vtrans[0] = Screen_Round(SCREEN_CENTRE_X * 4 + (vp->vp.vtrans[0] - SCREEN_CENTRE_X * 4) * sScreenFill);
    copy->vp.vtrans[1] = Screen_Round(SCREEN_CENTRE_Y * 4 + (vp->vp.vtrans[1] - sScreenCentreY * 4) * sScreenFill);
    gfx->words.w1 = (u32) copy;
}

// The list from list up to its end, copied into the arena, or NULL if it has none in reach or doesn't fit.
static Gfx* Screen_Copy(Gfx* list) {
    Gfx* copy;
    s32 count;
    s32 i;

    for (count = 0; count < SCREEN_MAX_COMMANDS; count++) {
        u32 op = list[count].words.w0 >> 24;
        if ((op == G_ENDDL) || ((op == G_DL) && (((list[count].words.w0 >> 16) & 0xFF) == G_DL_NOPUSH))) {
            break;
        }
    }
    if (count == SCREEN_MAX_COMMANDS) {
        return NULL;
    }
    copy = Screen_Alloc(count + 1);
    if (copy == NULL) {
        return NULL;
    }
    for (i = 0; i <= count; i++) {
        copy[i] = list[i];
    }
    return copy;
}

// The commands from gfx to end, or to the list's end with end NULL, scaled, or with change false only read: the
// cycle type and the segments followed as the RSP would. Whether there is anything to scale in them.
static bool Screen_Walk(Gfx* gfx, Gfx* end, bool change, s32 depth) {
    bool found = false;
    s32 count;

    for (count = 0; ((end == NULL) || (gfx < end)) && (count < SCREEN_MAX_COMMANDS); gfx++, count++) {
        u32 w0 = gfx->words.w0;
        u32 w1 = gfx->words.w1;
        bool inclusive = (sScreenCycleType == G_CYC_COPY) || (sScreenCycleType == G_CYC_FILL);

        switch (w0 >> 24) {
            case G_ENDDL:
                return found;
            case G_DL: {
                bool branch = ((w0 >> 16) & 0xFF) == G_DL_NOPUSH;
                Gfx* list = Screen_Resolve(w1);
                u32 segments[16];
                u32 cycleType = sScreenCycleType;
                Gfx* copy;
                s32 i;

                if ((list != NULL) && (depth < SCREEN_MAX_DEPTH)) {
                    for (i = 0; i < 16; i++) {
                        segments[i] = sScreenSegments[i];
                    }
                    if (Screen_Walk(list, NULL, false, depth + 1)) {
                        found = true;
                        copy = change ? Screen_Copy(list) : NULL;
                        if (copy != NULL) {
                            for (i = 0; i < 16; i++) {
                                sScreenSegments[i] = segments[i];
                            }
                            sScreenCycleType = cycleType;
                            Screen_Walk(copy, NULL, true, depth + 1);
                            gfx->words.w1 = (u32) copy;
                        }
                    }
                }
                if (branch) {
                    return found;
                }
                break;
            }
            case G_MOVEWORD:
                if ((((w0 >> 16) & 0xFF) == G_MW_SEGMENT) && ((w0 & 0xFFFF) < 16 * 4)) {
                    sScreenSegments[(w0 & 0xFFFF) / 4] = w1 & 0xFFFFFF;
                }
                break;
            case G_RDPSETOTHERMODE:
                sScreenCycleType = w0 & (3 << G_MDSFT_CYCLETYPE);
                break;
            case G_SETOTHERMODE_H:
                if (w0 == SCREEN_SET_CYCLE_TYPE) {
                    sScreenCycleType = w1;
                }
                break;
            case G_SETSCISSOR:
                found = true;
                if (change) {
                    Screen_ScaleScissor(gfx);
                }
                break;
            case G_MOVEMEM:
                if (w0 == SCREEN_MOVEMEM_VIEWPORT) {
                    found = true;
                    if (change) {
                        Screen_ScaleViewport(gfx);
                    }
                }
                break;
            case G_FILLRECT:
                found = true;
                if (change) {
                    Screen_ReplaceFill(gfx, inclusive);
                }
                break;
            case G_TEXRECT:
                if (((gfx[1].words.w0 >> 24) == G_RDPHALF_1) && ((gfx[2].words.w0 >> 24) == G_RDPHALF_2)) {
                    found = true;
                    if (change) {
                        Screen_ReplaceRect(gfx, inclusive);
                    }
                    gfx += 2;
                    count += 2;
                }
                break;
        }
    }
    return found;
}

void Screen_Fill(Gfx* start, Gfx* end, f32 fill, f32 centreY) {
    s32 i;

    sScreenArena = (sScreenArena + 1) % SCREEN_ARENA_COUNT;
    sScreenArenaUsed = 0;
    sScreenFill = fill;
    sScreenCentreY = centreY;
    sScreenCycleType = G_CYC_1CYCLE;
    for (i = 0; i < 16; i++) {
        sScreenSegments[i] = gSegments[i] & 0xFFFFFF;
    }
    Screen_Walk(start, end, true, 0);
}
