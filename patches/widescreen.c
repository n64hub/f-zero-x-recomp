// What a picture wider than 4:3 needs (RT64's Aspect Ratio Expand, the window's shape): the race's sky reaching the
// window's edges, and the race drawn to them (Camera_Draw, below).
//
// RT64 widens the game's 3D to the window, but the sky behind a course is a quad of the game's own, put each frame just
// inside the camera's view (Background_Update and Background_UpdateSkyboxVtx, src/overlays/ovl_i3/background.c): half
// as wide as the view at its depth and a fifth more, about 5% past a 4:3 picture's edges. At 16:9 it ended there, and
// what lay beside it was whatever the colour buffer held, since the game clears only its depth. So the sky's width, and
// with it the venue's floor and clouds, which use the same range, is the game's times the window's aspect ratio over
// 4:3; their height stays the game's, since the sky's gradient runs from its top to its bottom. With two players the
// game puts the sky nearer (Background_InitBackgroundInfo: 5300 deep, its range from 4300 + 100), and its quad is 0.4%
// narrower than the view there, which the console's border hides: widened, the window showed beside it, 2 or 3 columns
// of an old frame at a 1680x720 window's sides. So in a picture wider than 4:3 the range is at least the view's at the
// sky's depth with the margin one player's has (7320 for a view 7000 wide), before it is widened.

#include "patches.h"
#include "fzx_game.h"
#include "overlays/ovl_i3/background.h"
#include "overlays/ovl_i2/transition.h"
#include "rt64_extended_gbi.h"
#include "screen.h"

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
        // @recomp At least as wide as the view at the sky's depth, with one player's margin, then widened.
        if (widen > 1.0f) {
            background->horizontalRange =
                MAX(background->horizontalRange, background->skyboxDepth * depthFovRatio * (7320.0f / 7000.0f));
        }
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
// Records and the GP's ending (their screens are the game's 12, 8, 308, 232 frame, which func_80069698 below scales)
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

// The decomp's func_80069698 (src/game/game.c), which draws a screen and the transition over it, with a screen drawn in
// the game's 12, 8, 308, 232 frame (the title, the menus, Records, the GP's ending) scaled to the picture's full height
// with its transition (screen.c), and the whole picture cleared to black first for the first three frames of each
// screen in a picture wider than 4:3. A framed screen keeps its shape, so beside it, and beside three or four
// players' quarters and the lines above and between two players' halves, nothing is drawn, and the last screen's
// picture stayed there. On a console nothing is drawn there and it stays black, and three frames clear each of the
// game's three framebuffers once.
//
// Every transition (src/overlays/ovl_i2/transition.c) draws in the frame too: black, a fade's colour, or the picture it
// leaves in pieces (its background, which Transition_SetBackgroundBuffer copies from the framebuffer last shown). On a
// framed screen it is scaled with the screen, so it covers the scaled screen as it covers the frame on a console, and
// its background is the frame as it was before it was scaled (below). A race, which draws to the edges itself, has
// only fades (Transition_HideSet, Transition_AppearSet), drawn to the edges below.
//
// A framed screen's models are placed in its 4:3 picture (gEXSetViewportAlign's centre origin for its viewports, which
// RT64 then takes as from the screen's middle, so the game's own from its left edge are moved back by half its width,
// in quarter pixels). RT64 widens a viewport to the window when it spans the screen's width, clipped by its clip ratio,
// and machine select draws each of its 30 machines in a viewport as big as the screen with its middle at the machine's
// slot: the grid was spread across the window, wider the wider it was, so the machine under the cursor stood away from
// it, and at 32:9 the first columns stood outside the picture.
extern s32 gGameModeChangeState;
extern s16 D_i2_80106DA0;
extern Gfx* (*sGamemodeDrawFuncs[])(Gfx*);

static s32 sClearedMode = -1;
static s32 sClearFrames = 0;

// The menus' frame, the game's 12, 8, 308, 232, which the transitions' background is too (transition.h).
#define FRAME_LEFT 12
#define FRAME_TOP 8

extern s16 D_800CCFE4;
extern s32 D_800DCCFC;
extern s32 D_800DCD00;
extern s32 D_800DCD04;
extern s32 D_800DCD08;
extern FrameBuffer* gFrameBuffers[3];
extern Transition sTransition;
extern u16* sTransitionPalettePtr;
extern Gfx D_8014888[];

// How much the last screen drawn in each of the game's three framebuffers was scaled, 0 if not at all, and about which
// line.
static f32 sFrameBufferFill[3];
static f32 sFrameBufferCentreY[3];

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

// The decomp's Transition_SetBackgroundBuffer (src/overlays/ovl_i2/transition.c), which copies the frame from the
// framebuffer last shown for a transition to draw in pieces, taking a scaled screen's pixels from where each was scaled
// to: the transition is scaled again with the screen (func_80069698), so it starts from the picture as it was shown and
// not from its middle scaled twice. The framebuffer in RAM holds the scaled screen's 4:3 middle, since RT64 draws what
// it copies to RAM at the game's own 4:3. The runtime has no data cache to invalidate first.
RECOMP_PATCH void Transition_SetBackgroundBuffer(void) {
    Transition* transition = &sTransition;
    u16* backgroundBufferPtr;
    FrameBuffer* frameBuffer;
    s32 index;
    s32 row;
    s32 column;
    f32 fill;
    s32 x;
    s32 y;

    if (!(transition->flags & TRANSITION_FLAG_SET_BACKGROUND_BUFFER)) {
        return;
    }

    transition->flags &= ~TRANSITION_FLAG_SET_BACKGROUND_BUFFER;
    backgroundBufferPtr = transition->backgroundBuffer;
    index = (D_800CCFE4 == 3) ? D_800DCD08 : D_800DCD00;
    frameBuffer = gFrameBuffers[index];
    fill = sFrameBufferFill[index];

    for (row = 0; row < TRANSITION_BACKGROUND_HEIGHT; row++) {
        for (column = 0; column < TRANSITION_BACKGROUND_WIDTH; column++, backgroundBufferPtr++) {
            if (fill > 0.0f) {
                x = SCREEN_WIDTH / 2 + (s32) ((column + FRAME_LEFT + 0.5f - SCREEN_WIDTH / 2) * fill);
                y = SCREEN_HEIGHT / 2 + (s32) ((row + FRAME_TOP + 0.5f - sFrameBufferCentreY[index]) * fill);
                x = MIN(MAX(x, 0), SCREEN_WIDTH - 1);
                y = MIN(MAX(y, 0), SCREEN_HEIGHT - 1);
                *backgroundBufferPtr = frameBuffer->array[y][x];
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
// race is: a rectangle as wide as its scissor, the whole 320, which RT64 stretches to the window's width. Extended
// origins would place it from a 16:9 picture's edges (HUD Placement), short of a wider window's. A framed screen's is
// scaled with the screen (func_80069698).
RECOMP_PATCH Gfx* Transition_FadeDraw(Gfx* gfx, Transition* transition) {
    TransitionInfo* fadeInfo = &transition->transitionInfo;

    gSPDisplayList(gfx++, D_8014888);

    if (transition->activeTransitionType == TRANSITION_TYPE_STATIC_FADE) {
        gDPSetAlphaCompare(gfx++, G_AC_DITHER);
    }

    gDPSetPrimColor(gfx++, 0, 0, FADE_RED(fadeInfo), FADE_GREEN(fadeInfo), FADE_BLUE(fadeInfo), FADE_ALPHA(fadeInfo));

    if (!Widescreen_IsFramedScreen(GET_MODE(gGameMode)) && (recomp_get_aspect_ratio(4.0f / 3.0f) > 4.0f / 3.0f)) {
        gEXPushScissor(gfx++);
        gDPSetScissor(gfx++, G_SC_NON_INTERLACE, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
        gDPFillRectangle(gfx++, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
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
    // @recomp How much the frame drawn in this framebuffer, the screen's or only a transition's, is scaled, 0 if not.
    f32 centreY = SCREEN_HEIGHT / 2.0f;
    f32 fill = Widescreen_IsFramedScreen(GET_MODE(gGameMode)) ? Screen_GetFill(GET_MODE(gGameMode), &centreY) : 0.0f;
    s32 index = (D_800CCFE4 == 2) ? D_800DCCFC : D_800DCD04;
    Gfx* start;

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
            // As wide as its scissor, which RT64 stretches to the window's width, as Transition_FadeDraw's: extended
            // origins would place it from a 16:9 picture's edges (HUD Placement), short of a wider window's. A fill's
            // lrx is inclusive, and RT64's Fix LR with Scissor (on by default) takes it to the scissor's edge.
            gDPSetScissor(gfx++, G_SC_NON_INTERLACE, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
            gDPFillRectangle(gfx++, 0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1);
            gDPPipeSync(gfx++);
            gEXPopScissor(gfx++);
            gEXPopOtherMode(gfx++);
        }
    }
    // @recomp A framed screen's viewports in its picture, its models with them (above).
    if (fill > 0.0f) {
        gEXSetViewportAlign(gfx++, G_EX_ORIGIN_CENTER, -(SCREEN_WIDTH / 2) * 4, 0);
    }
    start = gfx;
    if (drawn) {
        gfx = sGamemodeDrawFuncs[GET_MODE(gGameMode)](gfx);
    }
    gfx = Transition_Draw(gfx);
    // @recomp A framed screen scaled to the picture's full height, with the transition drawn over it.
    if (drawn || transition) {
        if (fill > 0.0f) {
            Screen_Fill(start, gfx, fill, centreY);
        }
        sFrameBufferFill[index] = fill;
        sFrameBufferCentreY[index] = centreY;
    }
    if (fill > 0.0f) {
        gEXSetViewportAlign(gfx++, G_EX_ORIGIN_NONE, 0, 0);
    }
    return gfx;
}
