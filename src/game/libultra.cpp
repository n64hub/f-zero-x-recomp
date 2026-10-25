// libultra functions the game links that N64Recomp leaves out (its ignored_funcs) and the runtime doesn't implement,
// so each call to one in RecompiledFuncs/ comes here. The 64DD's are reached only from code the runtime never runs
// with a drive (patches/leo.c), and from a function of the game's that only links libultra in (EA90.c's
// func_80074F98), but they link.
#include <cstdint>

#include "recomp.h"

// s64 >> s64, for the game's own 64-bit arithmetic: a and b in a0:a1 and a2:a3, the result in v0:v1.
extern "C" void __ll_rshift_recomp(uint8_t* rdram, recomp_context* ctx) {
    int64_t a = (int64_t)((ctx->r4 << 32) | ((ctx->r5 << 0) & 0xFFFFFFFFu));
    uint64_t b = (ctx->r6 << 32) | ((ctx->r7 << 0) & 0xFFFFFFFFu);
    int64_t ret = a >> b;

    ctx->r2 = (int32_t)(ret >> 32);
    ctx->r3 = (int32_t)(ret >> 0);
}

// The 5-bit CRC of a Controller Pak address, as libultra's io/crc.c computes it, for the rumble's commands.
extern "C" void __osContAddressCrc_recomp(uint8_t* rdram, recomp_context* ctx) {
    uint32_t addr = (uint16_t)ctx->r4;
    uint32_t ret = 0;
    for (uint32_t bit = 0x400; bit != 0; bit /= 2) {
        ret *= 2;
        if (addr & bit) {
            if (ret & 0x20) {
                ret ^= 0x14;
            } else {
                ret++;
            }
        } else if (ret & 0x20) {
            ret ^= 0x15;
        }
    }
    for (int i = 0; i < 5; i++) {
        ret <<= 1;
        if (ret & 0x20) {
            ret ^= 0x15;
        }
    }
    ctx->r2 = ret & 0x1F;
}

// Every interrupt enabled (OS_IM_ALL).
extern "C" void osGetIntMask_recomp(uint8_t* rdram, recomp_context* ctx) {
    ctx->r2 = 0x003FFF01;
}

// No 64DD: there is no handle to its registers, and a write to one goes nowhere.
extern "C" void osLeoDiskInit_recomp(uint8_t* rdram, recomp_context* ctx) {
    ctx->r2 = 0;
}

extern "C" void osEPiWriteIo_recomp(uint8_t* rdram, recomp_context* ctx) {
    ctx->r2 = 0;
}
