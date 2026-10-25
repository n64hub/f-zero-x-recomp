// What RT64 needs to interpolate the game's frames to the display's rate: each camera's projection and view as two
// matrices, and an id for each camera and each racer's matrices, so a frame's are paired with the frame before's by who
// they are.
//
// The game loads each camera as one matrix, its projection times its view (Camera_UpdateProjectionViewMtx, unk_20208
// [camera id] in the gfx pool), and RT64 splits such a matrix in two by assuming a frustum centred on the screen. The
// game's isn't (its frustum has a centre, camera->frustrumCenterX/Y), so the view RT64 found was turned, and the
// frames between two of the game's drew whatever was near the camera, the player's machine most of all, turned away
// and back each game frame. Here each camera's load is its projection and then its view, in floats (gEXMatrixFloat),
// from the camera's own (camera->projectionMtx and viewMtx, whose product the game loads): the same matrix, which RT64
// keeps as the two it is.
//
// Untagged, RT64 pairs a frame's matrices with the frame before's by their draw calls and how close the matrices are,
// and the machines, all drawn from a few models, were paired with each other. Racer_Draw (src/game/racer.c, not matched
// C in the decomp) draws every racer, its shadow and its attack highlight with matrices at fixed places in the gfx pool,
// indexed by the racer's id (unk_20308, unk_20A88 and unk_21208).
//
// So rather than copies of Racer_Draw and the other functions that draw a camera's view, each frame's finished list
// (Gfx_FullSync) is read for the matrix loads from those places, and each one is replaced by a call to a list of a few
// commands: the group with its id, the load (a camera's in two), and the end. A group stays on until the next is set,
// so the first other matrix the list loads after a tagged one is wrapped the same way with RT64's own untagged group.
// The lists are this file's, a set for each of the game's two gfx pools, written while the game builds a frame in one
// pool and RT64 may still read the other's.

#include "patches.h"
#include "fzx_camera.h"
#include "rt64_extended_gbi.h"

// src/sys/sys_gfx.c's, which no header declares.
extern GfxPool* gGfxPool;
extern Gfx* gMasterDisp;
extern s32 D_800DCCFC;

// "FZ", the kind of matrix and the racer's or camera's index.
#define FZX_ID(kind, index) (0x465A0000u | ((kind) << 8) | (index))
#define KIND_MACHINE 1
#define KIND_SHADOW 2
#define KIND_HIGHLIGHT 3
#define KIND_CAMERA 4
#define KIND_BOOSTER 5

// A racer's boosters are drawn once for each player's view, so their id has the view too.
#define FZX_VIEW_ID(kind, view, index) (FZX_ID(kind, index) | ((u32) (view) << 12))

// A gfx pool member's segmented address: the game loads them through segment 1, set to the pool being built.
#define POOL_ADDRESS(member) (0x01000000u + (u32) __builtin_offsetof(GfxPool, member))

#define CAMERA_COUNT 4
#define RACER_COUNT 30

#define TAG_LIST_COUNT 512
#define TAG_LIST_LENGTH 8

static Gfx sTagLists[2][TAG_LIST_COUNT][TAG_LIST_LENGTH];
static MtxF sCameraMatrices[2][CAMERA_COUNT][2];
static Gfx* sListStart;

// The model matrix the game's effects are drawn with: their vertices are in world space.
static MtxF sIdentity = { { { 1.0f, 0.0f, 0.0f, 0.0f },
                            { 0.0f, 1.0f, 0.0f, 0.0f },
                            { 0.0f, 0.0f, 1.0f, 0.0f },
                            { 0.0f, 0.0f, 0.0f, 1.0f } } };

// src/game/racer.c's.
extern s32 gTotalRacers;
extern Gfx aSetupBoosterDL[];

// The index of a matrix in the gfx pool's array at start, of count, or -1.
static s32 pool_index(u32 address, u32 start, s32 count) {
    if ((address < start) || (address >= start + count * sizeof(Mtx)) || (((address - start) % sizeof(Mtx)) != 0)) {
        return -1;
    }

    return (address - start) / sizeof(Mtx);
}

// The id of a matrix in one of the arrays a racer's matrices are kept in, or 0.
static u32 racer_matrix_id(u32 address) {
    static const struct {
        u32 start;
        u32 kind;
    } arrays[] = {
        { POOL_ADDRESS(unk_20308), KIND_MACHINE },
        { POOL_ADDRESS(unk_20A88), KIND_SHADOW },
        { POOL_ADDRESS(unk_21208), KIND_HIGHLIGHT },
    };
    s32 i;

    for (i = 0; i < ARRAY_COUNT(arrays); i++) {
        s32 index = pool_index(address, arrays[i].start, RACER_COUNT);

        if (index >= 0) {
            return FZX_ID(arrays[i].kind, index);
        }
    }

    return 0;
}

static Camera* camera_by_id(s32 id) {
    s32 i;

    for (i = 0; i < CAMERA_COUNT; i++) {
        if (gCameras[i].id == id) {
            return &gCameras[i];
        }
    }

    return NULL;
}

// As RT64 has an untagged model matrix (TransformGroup's defaults), each component as its checks say, but a racer's
// not decomposed: its matrix is scaled on each axis from the machine's.
static Gfx* write_model_group(Gfx* gfx, u32 id) {
    gEXMatrixGroup(gfx++, id, (id != G_EX_ID_AUTO) ? G_EX_INTERPOLATE_SIMPLE : G_EX_INTERPOLATE_DECOMPOSE, G_EX_NOPUSH,
                   G_MTX_MODELVIEW, G_EX_COMPONENT_AUTO, G_EX_COMPONENT_AUTO, G_EX_COMPONENT_AUTO, G_EX_COMPONENT_AUTO,
                   G_EX_COMPONENT_AUTO, G_EX_COMPONENT_SKIP, G_EX_COMPONENT_AUTO, G_EX_ORDER_AUTO, G_EX_EDIT_NONE,
                   G_EX_ASPECT_AUTO, G_EX_COMPONENT_SKIP, G_EX_COMPONENT_AUTO);
    return gfx;
}

// As RT64 has an untagged camera: not decomposed, every component interpolated.
static Gfx* write_projection_group(Gfx* gfx, u32 id) {
    gEXMatrixGroup(gfx++, id, G_EX_INTERPOLATE_SIMPLE, G_EX_NOPUSH, G_MTX_PROJECTION, G_EX_COMPONENT_INTERPOLATE,
                   G_EX_COMPONENT_INTERPOLATE, G_EX_COMPONENT_INTERPOLATE, G_EX_COMPONENT_INTERPOLATE,
                   G_EX_COMPONENT_INTERPOLATE, G_EX_COMPONENT_SKIP, G_EX_COMPONENT_AUTO, G_EX_ORDER_AUTO,
                   G_EX_EDIT_NONE, G_EX_ASPECT_AUTO, G_EX_COMPONENT_SKIP, G_EX_COMPONENT_AUTO);
    return gfx;
}

// A camera's projection and view, in that order, which is the matrix the game loads: gEXMatrixFloat's flags go to RT64
// as gSPMatrix's do in F3DEX2, the push flag inverted, which a projection doesn't use.
static Gfx* write_camera(Gfx* gfx, Camera* camera, s32 index) {
    MtxF* matrices = sCameraMatrices[D_800DCCFC & 1][index];

    matrices[0] = camera->projectionMtx;
    matrices[1] = camera->viewMtx;
    gEXMatrixFloat(gfx++, OS_K0_TO_PHYSICAL(&matrices[0]), G_MTX_LOAD | G_MTX_PROJECTION);
    gEXMatrixFloat(gfx++, OS_K0_TO_PHYSICAL(&matrices[1]), G_MTX_MUL | G_MTX_PROJECTION);
    return gfx;
}

static void tag_list(Gfx* start, Gfx* end) {
    Gfx(*lists)[TAG_LIST_LENGTH] = sTagLists[D_800DCCFC & 1];
    s32 count = 0;
    bool modelTagged = false;
    bool projectionTagged = false;
    Gfx* gfx;

    for (gfx = start; (gfx < end) && (count < TAG_LIST_COUNT); gfx++) {
        u32 w0 = gfx->words.w0;
        u32 address = gfx->words.w1;
        Camera* camera = NULL;
        s32 cameraIndex = -1;
        bool projection;
        u32 id;
        Gfx* list;
        Gfx* cmd;

        if ((w0 >> 24) != (u8) G_MTX) {
            continue;
        }

        projection = (((w0 & 0xFF) ^ G_MTX_PUSH) & G_MTX_PROJECTION) != 0;
        if (projection) {
            if ((((w0 & 0xFF) ^ G_MTX_PUSH) & G_MTX_LOAD) != 0) {
                cameraIndex = pool_index(address, POOL_ADDRESS(unk_20208), CAMERA_COUNT);
            }

            camera = (cameraIndex >= 0) ? camera_by_id(cameraIndex) : NULL;
            id = (camera != NULL) ? FZX_ID(KIND_CAMERA, cameraIndex) : 0;
            if ((id == 0) && !projectionTagged) {
                continue;
            }

            projectionTagged = (id != 0);
        } else {
            id = racer_matrix_id(address);
            if ((id == 0) && !modelTagged) {
                continue;
            }

            modelTagged = (id != 0);
        }

        list = lists[count++];
        cmd = list;
        if (projection) {
            cmd = write_projection_group(cmd, (id != 0) ? id : G_EX_ID_AUTO);
        } else {
            cmd = write_model_group(cmd, (id != 0) ? id : G_EX_ID_AUTO);
        }

        if (camera != NULL) {
            cmd = write_camera(cmd, camera, cameraIndex);
        } else {
            *cmd++ = *gfx;
        }

        gSPEndDisplayList(cmd++);
        gSPDisplayList(gfx, OS_K0_TO_PHYSICAL(list));
    }
}

// src/sys/sys_gfx.c's for the US build, with RT64's extended commands enabled first: every frame's list starts here.
RECOMP_PATCH void Gfx_LoadSegments(void) {
    gEXEnable(gMasterDisp++);
    sListStart = gMasterDisp;
    gMasterDisp = Segment_SetTableAddresses(gMasterDisp);
}

// src/sys/sys_gfx.c's for the US build, then the frame's list tagged.
RECOMP_PATCH void Gfx_FullSync(void) {
    gDPFullSync(gMasterDisp++);
    gSPEndDisplayList(gMasterDisp++);

    if ((sListStart >= gGfxPool->gfxBuffer) && (sListStart < gMasterDisp)) {
        tag_list(sListStart, gMasterDisp);
    }

    sListStart = NULL;
}
