// What a picture wider than 4:3 needs (RT64's Aspect Ratio Expand, the window's shape): the race's sky reaching the
// window's edges, and the race drawn to them (Camera_Draw, below).
//
// RT64 widens the game's 3D to the window, but the sky behind a course is a quad of the game's own, put each frame just
// inside the camera's view (Background_Update and Background_UpdateSkyboxVtx, src/overlays/ovl_i3/background.c): half
// as wide as the view at its depth and a fifth more, about 5% past a 4:3 picture's edges. At 16:9 it ended there, and
// what lay beside it was whatever the colour buffer held, since the game clears only its depth. So the sky's width, and
// with it the venue's floor and clouds, which use the same range, is the game's times the window's aspect ratio over
// 4:3; their height stays the game's, since the sky's gradient runs from its top to its bottom.

#include "patches.h"
#include "fzx_game.h"
#include "overlays/ovl_i3/background.h"
#include "overlays/ovl_i2/transition.h"
#include "rt64_extended_gbi.h"

extern GfxPool* gGfxPool;
extern Background sBackgrounds[4];
extern s32 sBackgroundCount;
extern s16 sBackgroundSpriteCount;
extern u16 sSkyboxFlags;
extern s32 sCloudCount;
extern f32 sBackgroundScale;

// The decomp's Background_Update, with the ranges widened.
RECOMP_PATCH void Background_Update(void) {
    Camera* camera;
    Background* background;
    Vtx* vtx;
    s32 i;
    f32 xScale;
    f32 zScale;
    f32 depthFovRatio;
    f32 xRange;
    f32 zRange;
    f32 xOffset;
    f32 zOffset;
    f32 cloudHeightAboveEye;
    s32 angle;
    f32 cloudTransparencyStrength;
    bool usingHeightRelativeToEye;
    f32 widen = recomp_get_aspect_ratio(4.0f / 3.0f) / (4.0f / 3.0f);

    for (i = 0, background = sBackgrounds, camera = gCameras; i < sBackgroundCount; i++, background++, camera++) {

        background->pos.x = camera->eye.x + (camera->xzNormalizedX * sBackgroundScale);
        background->pos.z = camera->eye.z + (camera->xzNormalizedZ * sBackgroundScale);
        angle = Math_Round(DEG_TO_FZXANG3(0.5f * camera->fov));
        depthFovRatio = TAN(angle);

        background->horizontalRange = (background->scrollDepth + sBackgroundScale) * depthFovRatio * 1.2f;
        background->verticalRange = background->horizontalRange * background->aspectRatio;
        background->horizontalRange *= widen;

        xScale = camera->xzNormalizedX;
        zScale = camera->xzNormalizedZ;
        xOffset = background->scrollDepth * xScale;
        zOffset = background->scrollDepth * zScale;
        xRange = background->horizontalRange * zScale;
        zRange = background->horizontalRange * (0.0f - xScale);

        vtx = &gGfxPool->unk_29B48[i * 28 + 2 * 4];
        Background_UpdateSkyboxVtx(vtx, background, camera, depthFovRatio);

        vtx = &gGfxPool->unk_29B48[i * 28 + 0 * 4];
        Background_UpdateVenueFloorVtx(vtx, background, camera, xOffset, zOffset, 0.0f, 0.0f, xRange, zRange, 255, 0);

        vtx = &gGfxPool->unk_29B48[i * 28 + 1 * 4];
        Background_UpdateVenueFloorVtx(vtx, background, camera, 0.0f, 0.0f, 0.0f - xOffset, 0.0f - zOffset, xRange,
                                       zRange, 0, 255);

        if (sCloudCount > 0) {
            cloudHeightAboveEye =
                (background->pos.y + background->cloudScroll.relativeBackgroundHeight) - camera->eye.y;
            if (cloudHeightAboveEye == 0.0f) {
                continue;
            }
            cloudTransparencyStrength = ABS(cloudHeightAboveEye);
            cloudTransparencyStrength /= 100.0f;
            if (cloudTransparencyStrength > 1.0f) {
                cloudTransparencyStrength = 1.0f;
            }
            if (cloudHeightAboveEye > 0.0f) {
                usingHeightRelativeToEye = true;
            } else {
                usingHeightRelativeToEye = false;
            }

            vtx = &gGfxPool->unk_29B48[i * 28 + 3 * 4];
            Background_UpdateCloudVtx(vtx, background, camera, &background->cloudScroll, xOffset, zOffset, 0.0f, 0.0f,
                                      xRange, zRange, 255, ((1.0f - cloudTransparencyStrength) * 255.0f),
                                      usingHeightRelativeToEye);

            vtx = &gGfxPool->unk_29B48[i * 28 + 4 * 4];
            Background_UpdateCloudVtx(vtx, background, camera, &background->cloudScroll, 0.0f, 0.0f, 0.0f - xOffset,
                                      0.0f - zOffset, xRange, zRange, ((1.0f - cloudTransparencyStrength) * 255.0f),
                                      255, usingHeightRelativeToEye);
        }
        if (sSkyboxFlags & SKYBOX_STARRY) {
            Background_UpdateStars(i, background, camera);
        }
    }
    if (sBackgroundSpriteCount != 0) {
        Background_UpdateBackgroundSprites();
    }
}

// The decomp's Camera_Draw, with a view of one or two players opened to the picture's edges: the game's scissor keeps
// a race's 3D in from 4:3's (12, 16, 308, 224 with one player, the halves 12-307 with two), the border a console shows
// too, and RT64 keeps it as far in from the window's. Every one of a race's views is set here, its sky's and course's
// (Background_Draw) and its racers' (Race_Draw). The one-player view's viewport is the whole 320x240; the top half's
// starts 4 lines down, so its scissor does too, and the line between the halves stays. The game clears the depth only
// inside its own scissor, and what the race draws beside it showed nothing wrong at 4K in Chrome; a second clear over
// the whole picture cost a quarter of the presents there and set off a crash in RT64's render to RAM on the web.
// Records and the GP's ending (their screens are the game's 12, 8, 308, 232 frame, which func_80069698 below enlarges)
// and three or four players' quarters stay as the game has them, and so does a 4:3 picture.
extern s32 gGameMode;
extern s32 gNumPlayers;
extern GfxPool D_1000000;

RECOMP_PATCH Gfx* Camera_Draw(Gfx* gfx, s32 scissorBoxType, s32 cameraIndex) {
    Camera* camera = &gCameras[cameraIndex];
    // @recomp The top and bottom of the view opened to the picture's sides, or -1 to keep the game's.
    s32 top = -1;
    s32 bottom = -1;

    switch (camera->vpTransitionState) {
        case CAMERA_VP_TRANSITION_ACTIVE:
            gSPViewport(gfx++, &D_1000000.unk_2C2C8[cameraIndex]);
            gDPSetScissor(gfx++, G_SC_NON_INTERLACE, camera->currentScissorLeft, camera->currentScissorTop,
                          camera->currentScissorRight, camera->currentScissorBottom);
            break;
        default:
            gfx = func_8006A00C(gfx, scissorBoxType);
            if ((gGameMode != GAMEMODE_RECORDS) && (gGameMode != GAMEMODE_GP_END_CS) &&
                (recomp_get_aspect_ratio(4.0f / 3.0f) > 4.0f / 3.0f)) {
                switch (scissorBoxType) {
                    case SCISSOR_BOX_FULL_SCREEN:
                        if (gNumPlayers == 1) {
                            top = 0;
                            bottom = SCREEN_HEIGHT;
                        }
                        break;
                    case SCISSOR_BOX_TOP_HALF:
                        top = 4;
                        bottom = 119;
                        break;
                    case SCISSOR_BOX_BOTTOM_HALF:
                        top = 120;
                        bottom = SCREEN_HEIGHT;
                        break;
                }
            }
            if (top >= 0) {
                gEXSetScissor(gfx++, G_SC_NON_INTERLACE, G_EX_ORIGIN_LEFT, G_EX_ORIGIN_RIGHT, 0, top, 0, bottom);
            } else if ((gNumPlayers == 1) && (gGameMode != GAMEMODE_RECORDS) && (gGameMode != GAMEMODE_GP_END_CS)) {
                gDPSetScissor(gfx++, G_SC_NON_INTERLACE, 12, 16, 308, 224);
            }
            break;
    }

    return gfx;
}

// The decomp's func_80069698 (src/game/game.c), which draws a screen and the transition over it, in a picture wider than
// 4:3 with a screen drawn in the game's 12, 8, 308, 232 frame (the title, the menus, Records, the GP's ending) enlarged
// to the whole picture with its transition, and the whole picture cleared to black first for the first three frames of
// each screen. Races of one or two players, the title and the menus now draw to the picture's edges, while three or four
// players' quarters, and the lines above and between two players' halves, never clear there, so the last screen's
// picture stayed beside them. On a console nothing is drawn there and it stays black, and three frames clear each of
// the game's three framebuffers once.
//
// Every transition (src/overlays/ovl_i2/transition.c) draws in the frame too: black, a fade's colour, or the picture it
// leaves in pieces (its background, which Transition_SetBackgroundBuffer copies from the framebuffer last shown). On a
// framed screen it is drawn before the frame is enlarged, so it is enlarged with it and covers the whole picture as it
// covers the frame on a console, and its background is the frame as it was before it was enlarged (below). A race,
// which draws to the edges itself, has only fades (Transition_HideSet, Transition_AppearSet), drawn to the edges below.
extern s32 gGameModeChangeState;
extern s16 D_i2_80106DA0;
extern Gfx* (*sGamemodeDrawFuncs[])(Gfx*);

// RT64's G_EX_FILLRECT_V1, which rt64_extended_gbi.h has no macro for: a fill rectangle placed from the origins given
// (its src/gbi/rt64_gbi_extended.cpp, fillrectV1). RT64 drops one whose lrx is less than its ulx before it places them,
// so a rectangle to the right edge ends at 0 from it: a fill's right side is inclusive, and the scissor cuts it there.
#define gEXFillRectangle(cmd, lorigin, rorigin, ulx, uly, lrx, lry)                                                  \
    G_EX_COMMAND2(cmd, PARAM(RT64_EXTENDED_OPCODE, 8, 24) | PARAM(G_EX_FILLRECT_V1, 24, 0),                         \
                  PARAM(lorigin, 12, 0) | PARAM(rorigin, 12, 12), PARAM((ulx) * 4, 16, 16) | PARAM((uly) * 4, 16, 0), \
                  PARAM((lrx) * 4, 16, 16) | PARAM((lry) * 4, 16, 0))

static s32 sClearedMode = -1;
static s32 sClearFrames = 0;

// The menus' frame, the game's 12, 8, 308, 232, which the transitions' background is too (transition.h).
#define FRAME_LEFT 12
#define FRAME_TOP 8
#define FRAME_WIDTH (SCREEN_WIDTH - FRAME_LEFT * 2)
#define FRAME_HEIGHT (SCREEN_HEIGHT - FRAME_TOP * 2)
// Rows a load: 6 of the frame's 296 16-bit pixels fill most of TMEM's 4 KB.
#define FRAME_STRIP_ROWS 6

extern s16 D_800CCFE4;
extern s32 D_800DCCFC;
extern s32 D_800DCD00;
extern s32 D_800DCD04;
extern s32 D_800DCD08;
extern FrameBuffer* gFrameBuffers[3];
extern Transition sTransition;
extern u16* sTransitionPalettePtr;
extern Gfx D_8014888[];

// Whether each of the game's three framebuffers holds an enlarged frame, from the last screen drawn in it.
static bool sFrameBufferEnlarged[3];

// Whether a screen is drawn in the game's frame: all but the races', which draw to the picture's edges, and a race of
// three or four players keeps its own. The title is framed too: the attract demo races in the race's modes.
static bool Widescreen_IsFramedScreen(s32 mode) {
    switch (mode) {
        case GAMEMODE_GP_RACE:
        case GAMEMODE_PRACTICE:
        case GAMEMODE_VS_2P:
        case GAMEMODE_VS_3P:
        case GAMEMODE_VS_4P:
        case GAMEMODE_TIME_ATTACK:
        case GAMEMODE_DEATH_RACE:
            return false;
        default:
            return true;
    }
}

// The rows from row on, rows of them, of the frame drawn to where they are once the frame fills the picture: loaded
// from the framebuffer as a texture, which RT64 takes from what it drew there, at its resolution, as the load comes.
static Gfx* Widescreen_DrawFrameStrip(Gfx* gfx, void* framebuffer, s32 row, s32 rows) {
    s32 top = ((row - FRAME_TOP) * SCREEN_HEIGHT * 4) / FRAME_HEIGHT;
    s32 bottom = ((row + rows - FRAME_TOP) * SCREEN_HEIGHT * 4) / FRAME_HEIGHT;

    gDPLoadTextureTile(gfx++, framebuffer, G_IM_FMT_RGBA, G_IM_SIZ_16b, SCREEN_WIDTH, SCREEN_HEIGHT, FRAME_LEFT, row,
                       FRAME_LEFT + FRAME_WIDTH - 1, row + rows - 1, 0, G_TX_NOMIRROR | G_TX_CLAMP,
                       G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    // The texture's t where the rectangle's top lands, in 10.5 from its quarter pixels.
    gSPTextureRectangle(gfx++, 0, top, SCREEN_WIDTH << 2, bottom, G_TX_RENDERTILE, FRAME_LEFT << 5,
                        (FRAME_TOP << 5) + ((top * FRAME_HEIGHT) << 3) / SCREEN_HEIGHT,
                        (FRAME_WIDTH << 10) / SCREEN_WIDTH, (FRAME_HEIGHT << 10) / SCREEN_HEIGHT);
    return gfx;
}

// The frame a screen drew enlarged to the whole picture, in place: 320/296 as wide, which RT64 stretches to the window's
// width with the rest of the screen since it covers the scissor's, and 240/224 as high. The frame's top half is drawn
// from the top down and its bottom half from the bottom up, so each strip's rows are loaded before a strip is drawn
// over them.
static Gfx* Widescreen_FillFrame(Gfx* gfx) {
    void* framebuffer = (void*) OS_PHYSICAL_TO_K0(gFrameBuffers[(D_800CCFE4 == 2) ? D_800DCCFC : D_800DCD04]);
    s32 middle = FRAME_TOP + FRAME_HEIGHT / 2;
    s32 row;
    s32 rows;

    gEXPushOtherMode(gfx++);
    gEXPushCombineMode(gfx++);
    gEXPushScissor(gfx++);
    gfx = TextureUtils_SetFramebuffer(gfx);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPSetRenderMode(gfx++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
    gDPSetCombineMode(gfx++, G_CC_DECALRGB, G_CC_DECALRGB);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    gDPSetTextureLOD(gfx++, G_TL_TILE);
    gDPSetTextureDetail(gfx++, G_TD_CLAMP);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPSetTextureConvert(gfx++, G_TC_FILT);
    gDPSetAlphaCompare(gfx++, G_AC_NONE);
    gDPSetColorDither(gfx++, G_CD_DISABLE);
    gDPSetScissor(gfx++, G_SC_NON_INTERLACE, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    for (row = FRAME_TOP; row < middle; row += rows) {
        rows = MIN(FRAME_STRIP_ROWS, middle - row);
        gfx = Widescreen_DrawFrameStrip(gfx, framebuffer, row, rows);
    }
    for (row = FRAME_TOP + FRAME_HEIGHT; row > middle; row -= rows) {
        rows = MIN(FRAME_STRIP_ROWS, row - middle);
        gfx = Widescreen_DrawFrameStrip(gfx, framebuffer, row - rows, rows);
    }
    gDPPipeSync(gfx++);
    gEXPopScissor(gfx++);
    gEXPopCombineMode(gfx++);
    gEXPopOtherMode(gfx++);
    return gfx;
}

// The decomp's Transition_SetBackgroundBuffer (src/overlays/ovl_i2/transition.c), which copies the frame from the
// framebuffer last shown for a transition to draw in pieces, taking an enlarged frame's pixels from where each was
// enlarged to: the transition is enlarged again with the frame (func_80069698), so it starts from the picture as it was
// shown and not from its middle enlarged twice. The framebuffer in RAM holds all of the enlarged frame, since RT64 draws
// what it copies to RAM at the game's own 4:3. The runtime has no data cache to invalidate first.
RECOMP_PATCH void Transition_SetBackgroundBuffer(void) {
    Transition* transition = &sTransition;
    u16* backgroundBufferPtr;
    FrameBuffer* frameBuffer;
    s32 index;
    s32 row;
    s32 column;

    if (!(transition->flags & TRANSITION_FLAG_SET_BACKGROUND_BUFFER)) {
        return;
    }

    transition->flags &= ~TRANSITION_FLAG_SET_BACKGROUND_BUFFER;
    backgroundBufferPtr = transition->backgroundBuffer;
    index = (D_800CCFE4 == 3) ? D_800DCD08 : D_800DCD00;
    frameBuffer = gFrameBuffers[index];

    for (row = 0; row < TRANSITION_BACKGROUND_HEIGHT; row++) {
        for (column = 0; column < TRANSITION_BACKGROUND_WIDTH; column++, backgroundBufferPtr++) {
            if (sFrameBufferEnlarged[index]) {
                *backgroundBufferPtr = frameBuffer->array[((2 * row + 1) * SCREEN_HEIGHT) / (2 * FRAME_HEIGHT)]
                                                         [((2 * column + 1) * SCREEN_WIDTH) / (2 * FRAME_WIDTH)];
            } else {
                *backgroundBufferPtr = frameBuffer->array[row + FRAME_TOP][column + FRAME_LEFT];
            }
        }
    }

    if (transition->flags & TRANSITION_FLAG_GREYSCALE) {
        TextureUtils_ConvertToGreyscale(transition->backgroundBuffer,
                                        TRANSITION_BACKGROUND_WIDTH * TRANSITION_BACKGROUND_HEIGHT * sizeof(u16));
    }
    if (transition->flags & TRANSITION_FLAG_INVERSE_GREYSCALE) {
        Graphics_ConvertToInverseGreyscale(transition->backgroundBuffer,
                                           TRANSITION_BACKGROUND_WIDTH * TRANSITION_BACKGROUND_HEIGHT * sizeof(u16));
    }
    if (transition->flags & TRANSITION_FLAG_CONVERT_TO_PALETTE) {
        TextureUtils_GeneratePalette(transition->backgroundBuffer, sTransitionPalettePtr,
                                     TRANSITION_BACKGROUND_WIDTH * TRANSITION_BACKGROUND_HEIGHT * sizeof(u16));
    }
}

// The decomp's Transition_FadeDraw, with a race's fade, in a picture wider than 4:3, drawn to the picture's edges as the
// race is. A framed screen's is enlarged with its frame.
RECOMP_PATCH Gfx* Transition_FadeDraw(Gfx* gfx, Transition* transition) {
    TransitionInfo* fadeInfo = &transition->transitionInfo;

    gSPDisplayList(gfx++, D_8014888);

    if (transition->activeTransitionType == TRANSITION_TYPE_STATIC_FADE) {
        gDPSetAlphaCompare(gfx++, G_AC_DITHER);
    }

    gDPSetPrimColor(gfx++, 0, 0, FADE_RED(fadeInfo), FADE_GREEN(fadeInfo), FADE_BLUE(fadeInfo), FADE_ALPHA(fadeInfo));

    if (!Widescreen_IsFramedScreen(GET_MODE(gGameMode)) && (recomp_get_aspect_ratio(4.0f / 3.0f) > 4.0f / 3.0f)) {
        gEXPushScissor(gfx++);
        gEXSetScissor(gfx++, G_SC_NON_INTERLACE, G_EX_ORIGIN_LEFT, G_EX_ORIGIN_RIGHT, 0, 0, 0, SCREEN_HEIGHT);
        gEXFillRectangle(gfx++, G_EX_ORIGIN_LEFT, G_EX_ORIGIN_RIGHT, 0, 0, 0, SCREEN_HEIGHT);
        gEXPopScissor(gfx++);
    } else {
        gDPFillRectangle(gfx++, TRANSITION_BORDER_WIDTH, TRANSITION_BORDER_HEIGHT,
                         SCREEN_WIDTH - TRANSITION_BORDER_WIDTH, SCREEN_HEIGHT - TRANSITION_BORDER_HEIGHT);
    }

    return gfx;
}

RECOMP_PATCH Gfx* func_80069698(Gfx* gfx) {
    bool wide = recomp_get_aspect_ratio(4.0f / 3.0f) > 4.0f / 3.0f;
    bool drawn = (gGameModeChangeState != GAMEMODE_CHANGE_INIT) && D_i2_80106DA0;
    bool transition = sTransition.flags & TRANSITION_FLAG_DRAW;
    // @recomp Whether the frame drawn in this framebuffer, the screen's or only a transition's, is enlarged.
    bool enlarged = wide && Widescreen_IsFramedScreen(GET_MODE(gGameMode));
    s32 index = (D_800CCFE4 == 2) ? D_800DCCFC : D_800DCD04;

    if (drawn) {
        // @recomp Clear the picture for the first three frames of a screen.
        if (GET_MODE(gGameMode) != sClearedMode) {
            sClearedMode = GET_MODE(gGameMode);
            sClearFrames = 3;
        }
        if ((sClearFrames > 0) && wide) {
            sClearFrames--;
            gEXPushOtherMode(gfx++);
            gEXPushScissor(gfx++);
            gDPPipeSync(gfx++);
            gDPSetCycleType(gfx++, G_CYC_FILL);
            gDPSetRenderMode(gfx++, G_RM_NOOP, G_RM_NOOP2);
            gfx = TextureUtils_SetFramebuffer(gfx);
            gDPSetFillColor(gfx++, GPACK_RGBA5551(0, 0, 0, 1) << 16 | GPACK_RGBA5551(0, 0, 0, 1));
            gEXSetScissor(gfx++, G_SC_NON_INTERLACE, G_EX_ORIGIN_LEFT, G_EX_ORIGIN_RIGHT, 0, 0, 0, SCREEN_HEIGHT);
            gEXFillRectangle(gfx++, G_EX_ORIGIN_LEFT, G_EX_ORIGIN_RIGHT, 0, 0, 0, SCREEN_HEIGHT - 1);
            gDPPipeSync(gfx++);
            gEXPopScissor(gfx++);
            gEXPopOtherMode(gfx++);
        }
        gfx = sGamemodeDrawFuncs[GET_MODE(gGameMode)](gfx);
    }
    gfx = Transition_Draw(gfx);
    // @recomp A framed screen's frame enlarged to the picture, with the transition drawn over it.
    if (drawn || transition) {
        if (enlarged) {
            gfx = Widescreen_FillFrame(gfx);
        }
        sFrameBufferEnlarged[index] = enlarged;
    }
    return gfx;
}
