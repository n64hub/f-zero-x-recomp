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
// (Background_Draw) and its racers' (Race_Draw), and the depth they draw against is cleared over the whole picture
// first (fzx_race_clear_depth, src/game/widescreen.cpp). The one-player view's viewport is the whole 320x240; the top
// half's starts 4 lines down, so its scissor does too, and the line between the halves stays. Records and the GP's
// ending (their screens are the game's 12, 8, 308, 232 frame) and three or four players' quarters stay as the game
// has them, and so does a 4:3 picture.
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

// The decomp's func_80069698 (src/game/game.c), which draws a screen, with the whole picture cleared to black first for
// the first three frames of each screen in a picture wider than 4:3. A race now draws to the picture's edges, and the
// game's other screens draw only inside its 12, 8, 308, 232 frame and never clear outside it, so the last race's
// picture stayed there beside the next menu's. On a console nothing is drawn there and it stays black, and three
// frames clear each of the game's three framebuffers once.
extern s32 gGameModeChangeState;
extern s16 D_i2_80106DA0;
extern Gfx* (*sGamemodeDrawFuncs[])(Gfx*);

// RT64's G_EX_FILLRECT_V1, which rt64_extended_gbi.h has no macro for: a fill rectangle placed from the origins given
// (its src/gbi/rt64_gbi_extended.cpp, fillrectV1).
#define gEXFillRectangle(cmd, lorigin, rorigin, ulx, uly, lrx, lry)                                                  \
    G_EX_COMMAND2(cmd, PARAM(RT64_EXTENDED_OPCODE, 8, 24) | PARAM(G_EX_FILLRECT_V1, 24, 0),                         \
                  PARAM(lorigin, 12, 0) | PARAM(rorigin, 12, 12), PARAM((ulx) * 4, 16, 16) | PARAM((uly) * 4, 16, 0), \
                  PARAM((lrx) * 4, 16, 16) | PARAM((lry) * 4, 16, 0))

static s32 sClearedMode = -1;
static s32 sClearFrames = 0;

RECOMP_PATCH Gfx* func_80069698(Gfx* gfx) {

    if ((gGameModeChangeState != GAMEMODE_CHANGE_INIT) && D_i2_80106DA0) {
        // @recomp Clear the picture for the first three frames of a screen.
        if (GET_MODE(gGameMode) != sClearedMode) {
            sClearedMode = GET_MODE(gGameMode);
            sClearFrames = 3;
        }
        if ((sClearFrames > 0) && (recomp_get_aspect_ratio(4.0f / 3.0f) > 4.0f / 3.0f)) {
            sClearFrames--;
            gEXPushOtherMode(gfx++);
            gEXPushScissor(gfx++);
            gDPPipeSync(gfx++);
            gDPSetCycleType(gfx++, G_CYC_FILL);
            gDPSetRenderMode(gfx++, G_RM_NOOP, G_RM_NOOP2);
            gfx = TextureUtils_SetFramebuffer(gfx);
            gDPSetFillColor(gfx++, GPACK_RGBA5551(0, 0, 0, 1) << 16 | GPACK_RGBA5551(0, 0, 0, 1));
            gEXSetScissor(gfx++, G_SC_NON_INTERLACE, G_EX_ORIGIN_LEFT, G_EX_ORIGIN_RIGHT, 0, 0, 0, SCREEN_HEIGHT);
            gEXFillRectangle(gfx++, G_EX_ORIGIN_LEFT, G_EX_ORIGIN_RIGHT, 0, 0, -1, SCREEN_HEIGHT - 1);
            gDPPipeSync(gfx++);
            gEXPopScissor(gfx++);
            gEXPopOtherMode(gfx++);
        }
        gfx = sGamemodeDrawFuncs[GET_MODE(gGameMode)](gfx);
    }
    return Transition_Draw(gfx);
}
