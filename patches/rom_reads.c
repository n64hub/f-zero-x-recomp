#include "patches.h"
#include "fzx_racer.h"
#include "fzx_machine.h"

// F-Zero X reads words of its own ROM straight from the cartridge's address space (ROM_READ, at 0xB0000000 and up)
// rather than by DMA, each after a wait for the PI to be idle: a word of the boot code tells the Blue Falcon's first
// model whether the cartridge is genuine, another is read on the main menu (main_menu.c's unlock check), and the
// machines' handling is computed from 31 of them as a race starts. The runtime has no cartridge at those addresses, so
// these are the decomp's own functions with each read made through the runtime (recomp_rom_read, the word at that ROM
// offset) and the waits for the PI left out: there is no PI to wait for.

#undef ROM_READ
#define ROM_READ(addr) recomp_rom_read(addr)

// The Blue Falcon's model, from the decomp's assets (segment 3, machine_custom_gfx), by the addresses their names give.
#define D_30040F8 ((Vtx*) 0x030040F8)
#define D_3004238 ((Vtx*) 0x03004238)
#define D_3003918 ((Vtx*) 0x03003918)
#define D_3003758 ((Vtx*) 0x03003758)
#define D_3003978 ((Vtx*) 0x03003978)
#define D_3003468 ((Vtx*) 0x03003468)
#define D_30034E8 ((Vtx*) 0x030034E8)
#define D_30037D8 ((Vtx*) 0x030037D8)

extern Machine gMachines[30];
extern f32 D_800CF15C;
extern f32 D_800CF174[];
extern f32 D_800CF188[];
extern f32 D_800CF19C[];
f32 func_80089654(f32 arg0);

// game/racer.c
RECOMP_PATCH f32 Racer_InitMachineStats(Racer* racer, f32 arg1) {
    Machine* temp_a1;
    f32 temp_fs0;
    f32 temp_fv0;
    f32 temp_fs1;
    f32 temp_fv1;
    f32 temp_ret;
    f32 temp_fa1;

    temp_fs0 = (racer->unk_1F0 - 780.0f) / 1560.0f;

    temp_fv0 = ((((f32) ((ROM_READ(0x078) >> 0xF) & 0xFF) * 0.1f) -
                 ((f32) (((ROM_READ(0x50C) >> 0x13) & 0xFF) | 0x600) * 0.001f)) *
                temp_fs0) +
               ((f32) (((ROM_READ(0x50C) >> 0x13) & 0xFF) | 0x600) * 0.001f);
    temp_fv1 =
        ((((f32) ((ROM_READ(0x780) >> 0x15) & 0xFF) * 0.001f) - ((f32) ((ROM_READ(0x390) >> 9) & 0xFF) * 0.001f)) *
         temp_fs0) +
        ((f32) ((ROM_READ(0x390) >> 9) & 0xFF) * 0.001f);

    racer->unk_1B4 =
        ((temp_fv1 - temp_fv0) * arg1) + temp_fv0 + (0.2f * (temp_fv0 - temp_fv1) * (1.0f - SQ((2.0f * arg1) - 1.0f)));
    temp_fs1 = racer->unk_1B4;

    temp_fv0 = ((((f32) ((ROM_READ(0x0A0) >> 0x15) & 0xFF) * 0.1f) -
                 ((f32) (((ROM_READ(0x540) >> 0xD) & 0xFF) | 0x800) * 0.001f)) *
                temp_fs0) +
               ((f32) (((ROM_READ(0x540) >> 0xD) & 0xFF) | 0x800) * 0.001f);
    temp_fv1 =
        ((((f32) ((ROM_READ(0x95C) >> 0x13) & 0xFF) * 0.001f) - ((f32) ((ROM_READ(0x50C) >> 6) & 0xFF) * 0.001f)) *
         temp_fs0) +
        ((f32) ((ROM_READ(0x50C) >> 6) & 0xFF) * 0.001f);
    racer->unk_1B0 =
        ((temp_fv1 - temp_fv0) * arg1) + temp_fv0 + (0.2f * (temp_fv0 - temp_fv1) * (1.0f - SQ((2.0f * arg1) - 1.0f)));
    temp_fs1 += racer->unk_1B0;

    temp_fv0 =
        ((((f32) ((ROM_READ(0x1BC) >> 0x13) & 0xFF) * 0.001f) - ((f32) ((ROM_READ(0x57C) >> 0xF) & 0xFF) * 0.1f)) *
         temp_fs0) +
        ((f32) ((ROM_READ(0x57C) >> 0xF) & 0xFF) * 0.1f);
    temp_fv1 =
        (((((f32) ((ROM_READ(0x978) >> 0x15) & 0xFF) * 0.001f) - ((f32) ((ROM_READ(0x550) >> 0x13) & 0xFF) * 0.001f)) *
          temp_fs0)) +
        ((f32) ((ROM_READ(0x550) >> 0x13) & 0xFF) * 0.001f);
    racer->unk_1B8 = ((temp_fv1 - temp_fv0) * arg1) + temp_fv0;
    temp_fs1 += racer->unk_1B8;

    temp_fv0 = (((f32) ((ROM_READ(0x2C4) >> 0xE) & 0xFF) - (f32) ((ROM_READ(0x5D8) >> 4) & 0xFF)) * temp_fs0) +
               (f32) ((ROM_READ(0x5D8) >> 4) & 0xFF);
    temp_fv1 = (((f32) ((ROM_READ(0x988) >> 0x18) & 0xFF) - (f32) ((ROM_READ(0x57C) >> 0xA) & 0xFF)) * temp_fs0) +
               (f32) ((ROM_READ(0x57C) >> 0xA) & 0xFF);
    racer->unk_1BC = ((temp_fv1 - temp_fv0) * arg1) + temp_fv0;
    temp_fs1 += racer->unk_1BC;

    temp_fv0 =
        ((((f32) ((ROM_READ(0x2FC) >> 0x12) & 0xFF) * 0.001f) - ((f32) ((ROM_READ(0x660) >> 0x10) & 0xFF) * 0.001f)) *
         temp_fs0) +
        ((f32) ((ROM_READ(0x660) >> 0x10) & 0xFF) * 0.001f);
    temp_fv1 =
        (((((f32) ((ROM_READ(0xA20) >> 0xC) & 0xFF) * 0.01f) - ((f32) ((ROM_READ(0x5E4) >> 0x14) & 0xFF) * 0.01f)) *
          temp_fs0) +
         ((f32) ((ROM_READ(0x5E4) >> 0x14) & 0xFF) * 0.01f));
    racer->unk_1C8 =
        ((temp_fv1 - temp_fv0) * arg1) + temp_fv0 + (0.2f * (temp_fv1 - temp_fv0) * (1.0f - SQ((2.0f * arg1) - 1.0f)));
    temp_fs1 += racer->unk_1C8;

    temp_fv0 = (((f32) ((ROM_READ(0x36C) >> 0x10) & 0xFF) - (f32) ((ROM_READ(0x6A0) >> 0xC) & 0xFF)) * temp_fs0) +
               (f32) ((ROM_READ(0x6A0) >> 0xC) & 0xFF);
    temp_fv1 = ((((f32) ((ROM_READ(0x1BC) >> 0xA) & 0xFF) - (f32) ((ROM_READ(0x60C) >> 0x16) & 0xFF)) * temp_fs0) +
                (f32) ((ROM_READ(0x60C) >> 0x16) & 0xFF));
    racer->unk_1CC = racer->unk_1C8 / (((temp_fv1 - temp_fv0) * arg1) + temp_fv0 +
                                       (0.2f * (temp_fv1 - temp_fv0) * (1.0f - SQ((2.0f * arg1) - 1.0f))));
    temp_fs1 += racer->unk_1CC;

    temp_fv0 = ((((ROM_READ(0x374) >> 3) & 0xFF) - ((f32) ((ROM_READ(0x6D8) >> 2) & 0xFF) * 0.01f)) * temp_fs0) +
               (((ROM_READ(0x6D8) >> 2) & 0xFF) * 0.01f);
    temp_fv1 =
        (((((ROM_READ(0x1C8) >> 4) & 0xFF) * 0.01f) - ((f32) ((ROM_READ(0x664) >> 2) & 0xFF) * 0.01f)) * temp_fs0) +
        (((ROM_READ(0x664) >> 2) & 0xFF) * 0.01f);
    temp_a1 = &gMachines[racer->character];
    racer->unk_1F8 = D_800CF188[temp_a1->machineStats[GRIP_STAT]] + ((temp_fv1 - temp_fv0) * arg1) + temp_fv0;
    temp_fs1 += racer->unk_1F8;

    temp_fv0 =
        (((f32) ((ROM_READ(0x384) >> 5) & 0xFF) - ((f32) ((ROM_READ(0x718) >> 0xD) & 0xFF) * 0.001f)) * temp_fs0) +
        (((ROM_READ(0x718) >> 0xD) & 0xFF) * 0.001f);
    temp_fv1 =
        ((((f32) ((ROM_READ(0x248) >> 0xB) & 0xFF) * 0.001f) - ((f32) ((ROM_READ(0x6A0) >> 0xE) & 0xFF) * 0.01f)) *
         temp_fs0) +
        (((ROM_READ(0x6A0) >> 0xE) & 0xFF) * 0.01f);
    racer->unk_1FC = D_800CF19C[temp_a1->machineStats[GRIP_STAT]] + ((temp_fv1 - temp_fv0) * arg1) + temp_fv0;
    temp_fs1 += racer->unk_1FC;

    temp_fv0 = (((ROM_READ(0x38C) & 0xFF) - (((ROM_READ(0x740) >> 0x12) & 0xFF) * 0.001f)) * temp_fs0) +
               (((ROM_READ(0x740) >> 0x12) & 0xFF) * 0.001f);
    temp_fv1 =
        (((((ROM_READ(0x250) >> 3) & 0xFF) * 0.001f) - (((ROM_READ(0x718) >> 0x17) & 0xFF) * 0.001f)) * temp_fs0) +
        (((ROM_READ(0x718) >> 0x17) & 0xFF) * 0.001f);

    temp_fa1 = ((temp_fv1 - temp_fv0) * racer->unk_1A8) + temp_fv0;

    racer->unk_1C0 = (D_800CF174[temp_a1->machineStats[BOOST_STAT]] + temp_fa1) / racer->unk_1B8;
    temp_fs1 += racer->unk_1C0;
    racer->unk_1C4 = (D_800CF174[2] + temp_fa1) / racer->unk_1B8;
    temp_fs1 += racer->unk_1C4;
    temp_ret = func_80089654(arg1);
    temp_ret = func_80089654(temp_ret);
    temp_ret = func_80089654(temp_ret);
    racer->unk_1D0 = ((D_800CF15C - 1.0f) * temp_ret) + 1.0f;

    return temp_ret;
}

// overlays/ovl_i9/machine_draw.c
extern s16 D_800CE784;
extern s16 gGreyscaleMachinePart;
extern f32 gBodyHealthValues[];
extern OSMesgQueue D_800DCAB0;

RECOMP_PATCH Gfx* Machine_DrawBlueFalconLod1(Gfx* gfx) {
    OSMesg sp1AC;
    s32 temp_v1;
    s32 var_a1;
    u32 var_a0;

    osRecvMesg(&D_800DCAB0, &sp1AC, OS_MESG_NOBLOCK);
    osRecvMesg(&D_800DCAB0, &sp1AC, OS_MESG_BLOCK);
    var_a0 = 0;
    var_a1 = 0;
    while (var_a0 < 2) {
        temp_v1 = ROM_READ(0x510);
        if (temp_v1 == var_a1) {
            var_a0++;
        } else {
            var_a0 = 0;
        }
        var_a1 = temp_v1;
    }

    if (temp_v1 & 1) {
        D_800CE784++;
        if (D_800CE784 >= 13) {
            gBodyHealthValues[0] = gBodyHealthValues[1] = gBodyHealthValues[2] = gBodyHealthValues[3] =
                gBodyHealthValues[4] = 1.0f;
            func_i2_801039BC(var_a0);
        }
    }

    gDPSetCombineLERP(gfx++, TEXEL0, ENVIRONMENT, TEXEL0_ALPHA, ENVIRONMENT, 0, 0, 0, ENVIRONMENT, COMBINED, 0, SHADE,
                      0, 0, 0, 0, COMBINED);
    gSPTexture(gfx++, 0xFFFF, 0xFFFF, 0, 1, G_ON);
    gSPVertex(gfx++, D_30040F8, 20, 0);
    gSP2Triangles(gfx++, 0, 1, 2, 0, 3, 4, 5, 0);
    gSP2Triangles(gfx++, 4, 6, 5, 0, 7, 8, 9, 0);
    gSP2Triangles(gfx++, 10, 11, 12, 0, 13, 14, 15, 0);
    gSP2Triangles(gfx++, 13, 16, 14, 0, 17, 18, 19, 0);

    gDPPipeSync(gfx++);
#if BUILD_REVISION >= REVISION_C
    if (gGreyscaleMachinePart) {
        gDPSetPrimColor(gfx++, 0, 0, 228, 228, 228, 255);
    } else {
#endif
        gDPSetPrimColor(gfx++, 0, 0, 255, 255, 0, 255);
#if BUILD_REVISION >= REVISION_C
    }
#endif
    gDPSetCombineLERP(gfx++, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, 0, 0, 0, ENVIRONMENT, COMBINED, 0, SHADE, 0,
                      0, 0, 0, COMBINED);
    gSPTexture(gfx++, 0xFFFF, 0xFFFF, 0, 2, G_ON);
    gSPVertex(gfx++, D_3004238, 38, 0);
    gSP2Triangles(gfx++, 0, 1, 2, 0, 3, 0, 2, 0);
    gSP2Triangles(gfx++, 4, 5, 6, 0, 7, 8, 9, 0);
    gSP2Triangles(gfx++, 10, 11, 12, 0, 10, 12, 13, 0);
    gSP2Triangles(gfx++, 14, 15, 13, 0, 16, 17, 18, 0);
    gSP2Triangles(gfx++, 19, 20, 3, 0, 21, 14, 16, 0);
    gSP2Triangles(gfx++, 21, 15, 14, 0, 22, 23, 24, 0);
    gSP2Triangles(gfx++, 22, 24, 25, 0, 26, 27, 28, 0);
    gSP2Triangles(gfx++, 29, 30, 0, 0, 6, 31, 30, 0);
    gSP2Triangles(gfx++, 32, 6, 30, 0, 27, 33, 28, 0);
    gSP2Triangles(gfx++, 34, 35, 36, 0, 8, 37, 35, 0);
    gSP2Triangles(gfx++, 8, 35, 34, 0, 8, 7, 37, 0);

    gSPTexture(gfx++, 0xFFFF, 0xFFFF, 0, 3, G_ON);
    gSPVertex(gfx++, D_3003918, 6, 0);
    gSP2Triangles(gfx++, 0, 1, 2, 0, 3, 4, 5, 0);

    gDPPipeSync(gfx++);
    gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, 255);
    gSPTexture(gfx++, 0xFFFF, 0xFFFF, 0, 4, G_ON);
    gSPVertex(gfx++, D_3003758, 8, 0);
    gSP2Triangles(gfx++, 0, 1, 2, 0, 3, 4, 5, 0);
    gSP2Triangles(gfx++, 0, 2, 6, 0, 4, 7, 5, 0);

    gSPTexture(gfx++, 0xFFFF, 0xFFFF, 0, 5, G_ON);
    gSPVertex(gfx++, D_3003978, 64, 0);
    gSP2Triangles(gfx++, 0, 1, 2, 0, 3, 2, 1, 0);
    gSP2Triangles(gfx++, 4, 5, 3, 0, 6, 7, 8, 0);
    gSP2Triangles(gfx++, 9, 10, 11, 0, 9, 6, 10, 0);
    gSP2Triangles(gfx++, 12, 13, 14, 0, 15, 16, 17, 0);
    gSP2Triangles(gfx++, 18, 19, 20, 0, 21, 22, 23, 0);
    gSP2Triangles(gfx++, 21, 23, 24, 0, 25, 19, 26, 0);
    gSP2Triangles(gfx++, 25, 26, 27, 0, 28, 29, 30, 0);
    gSP2Triangles(gfx++, 28, 30, 26, 0, 18, 31, 19, 0);
    gSP2Triangles(gfx++, 32, 33, 34, 0, 35, 36, 34, 0);
    gSP2Triangles(gfx++, 37, 38, 39, 0, 13, 12, 40, 0);
    gSP2Triangles(gfx++, 41, 42, 43, 0, 44, 45, 46, 0);
    gSP2Triangles(gfx++, 47, 48, 49, 0, 50, 49, 51, 0);
    gSP2Triangles(gfx++, 50, 51, 52, 0, 53, 54, 55, 0);
    gSP2Triangles(gfx++, 53, 55, 56, 0, 57, 44, 33, 0);
    gSP2Triangles(gfx++, 57, 33, 58, 0, 57, 58, 59, 0);
    gSP1Triangle(gfx++, 60, 61, 40, 0);
    gSPVertex(gfx++, &D_3003978[62], 58, 0);
    gSP2Triangles(gfx++, 0, 1, 2, 0, 1, 3, 2, 0);
    gSP2Triangles(gfx++, 4, 5, 6, 0, 7, 8, 9, 0);
    gSP2Triangles(gfx++, 10, 11, 12, 0, 11, 10, 13, 0);
    gSP2Triangles(gfx++, 14, 15, 16, 0, 17, 18, 19, 0);
    gSP2Triangles(gfx++, 17, 20, 18, 0, 21, 18, 22, 0);
    gSP2Triangles(gfx++, 21, 19, 18, 0, 23, 17, 19, 0);
    gSP2Triangles(gfx++, 23, 24, 25, 0, 23, 19, 24, 0);
    gSP2Triangles(gfx++, 22, 26, 27, 0, 18, 26, 22, 0);
    gSP2Triangles(gfx++, 18, 28, 26, 0, 29, 30, 31, 0);
    gSP2Triangles(gfx++, 32, 33, 34, 0, 35, 36, 37, 0);
    gSP2Triangles(gfx++, 36, 38, 37, 0, 39, 40, 41, 0);
    gSP2Triangles(gfx++, 39, 42, 40, 0, 43, 44, 45, 0);
    gSP2Triangles(gfx++, 43, 46, 44, 0, 47, 48, 49, 0);
    gSP2Triangles(gfx++, 47, 50, 48, 0, 51, 52, 53, 0);
    gSP2Triangles(gfx++, 54, 55, 56, 0, 26, 56, 27, 0);
    gSP1Triangle(gfx++, 56, 55, 57, 0);

    gDPPipeSync(gfx++);
    gDPSetEnvColor(gfx++, 0, 0, 0, 255);
    gSPTexture(gfx++, 0xFFFF, 0xFFFF, 0, 6, G_ON);
    gSPVertex(gfx++, D_3003468, 8, 0);
    gSP2Triangles(gfx++, 0, 1, 2, 0, 1, 3, 2, 0);
    gSP2Triangles(gfx++, 4, 5, 6, 0, 4, 6, 7, 0);

    gSPTexture(gfx++, 0xFFFF, 0xFFFF, 0, 7, G_ON);
    gSPVertex(gfx++, D_30034E8, 39, 0);
    gSP2Triangles(gfx++, 0, 1, 2, 0, 3, 4, 5, 0);
    gSP2Triangles(gfx++, 6, 3, 5, 0, 7, 8, 9, 0);
    gSP2Triangles(gfx++, 10, 9, 11, 0, 12, 11, 13, 0);
    gSP2Triangles(gfx++, 12, 13, 14, 0, 15, 0, 16, 0);
    gSP2Triangles(gfx++, 17, 18, 19, 0, 20, 21, 22, 0);
    gSP2Triangles(gfx++, 20, 22, 23, 0, 24, 20, 23, 0);
    gSP2Triangles(gfx++, 22, 25, 26, 0, 27, 28, 29, 0);
    gSP2Triangles(gfx++, 27, 29, 30, 0, 22, 26, 31, 0);
    gSP2Triangles(gfx++, 32, 24, 23, 0, 33, 34, 35, 0);
    gSP2Triangles(gfx++, 33, 35, 36, 0, 33, 37, 38, 0);
    gSP2Triangles(gfx++, 33, 38, 34, 0, 0, 19, 1, 0);

    gDPPipeSync(gfx++);

#if BUILD_REVISION >= REVISION_C
    if (gGreyscaleMachinePart) {
        gDPSetPrimColor(gfx++, 0, 0, 232, 232, 232, 255);
        gDPSetEnvColor(gfx++, 81, 81, 81, 255);
    } else {
#endif
        gDPSetPrimColor(gfx++, 0, 0, 209, 238, 255, 255);
        gDPSetEnvColor(gfx++, 14, 99, 165, 255);
#if BUILD_REVISION >= REVISION_C
    }
#endif

    gSPTexture(gfx++, 0xFFFF, 0xFFFF, 0, 0, G_ON);
    gSPVertex(gfx++, D_30037D8, 20, 0);
    gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 3, 1, 0);
    gSP2Triangles(gfx++, 4, 5, 6, 0, 7, 8, 9, 0);
    gSP2Triangles(gfx++, 10, 11, 12, 0, 13, 14, 15, 0);
    gSP2Triangles(gfx++, 13, 16, 14, 0, 17, 18, 19, 0);

    return gfx;
}

// overlays/ovl_i6/E7CF0.c
RECOMP_PATCH s32 func_i6_8011DBD0(void) {
    u32 i = 0;
    s32 temp_a0;
    s32 var_v1 = 0;

    while (i < 2) {
        temp_a0 = ROM_READ(0x6E8);

        if (temp_a0 != var_v1) {
            i = 0;
        } else {
            i++;
        }

        var_v1 = temp_a0;
    }
    return temp_a0;
}
