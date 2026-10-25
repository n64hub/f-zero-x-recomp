#include "patches.h"

// The runtime registers the code inside ROM 0x1000 plus 1 MB at startup, which is main, and any other code only when
// it is told it was loaded (recomp_load_overlays). Every overlay the game loads comes through Dma_LoadOverlay
// (src/sys/dma.c): ovl_i2 and ovl_i10 at boot (sys_gfx.c), and the menus', the race's and the ending's overlays, which
// share one address, each time the game changes screen (segment.c, game.c). A 64DD's code is loaded other ways, but
// the runtime reports no 64DD.

// src/sys/dma.c's, which no header declares.
void Dma_RomCopy(u8* romAddr, u8* ramAddr, size_t size);
void Dma_RomCopyWithBssInit(u8* romAddr, u8* ramAddr, size_t size, void* bssAddr, size_t bssSize);

RECOMP_PATCH void Dma_LoadOverlay(u8* romAddr, u8* ramAddr, size_t size, void* bssAddr, size_t bssSize) {
    s32 remainder;
    s32 i;
    s32 numBlocks;

    // @recomp Register the code this brings in.
    recomp_load_overlays((u32) romAddr, ramAddr, size);

    numBlocks = size / 1024;

    for (i = 0; i < numBlocks; i++) {
        Dma_RomCopy(romAddr, ramAddr, 0x400);

        romAddr += 0x400;
        ramAddr += 0x400;
    }
    remainder = size % 1024;
    if (remainder != 0) {
        Dma_RomCopyWithBssInit(romAddr, ramAddr, remainder, bssAddr, bssSize);
    }
}
