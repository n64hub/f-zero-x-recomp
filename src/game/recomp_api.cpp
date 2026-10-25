// The natives the patches call (patches/syms.ld gives each a dummy address that N64Recomp turns into a call by name).

#include "recomp.h"
#include "librecomp/overlays.hpp"
#include "librecomp/helpers.hpp"
#include "librecomp/game.hpp"

// recomp_load_overlays(rom, ram, size): the code sections in that stretch of ROM are now at ram (patches/overlays.c).
extern "C" void recomp_load_overlays(uint8_t* rdram, recomp_context* ctx) {
    u32 rom = _arg<0, u32>(rdram, ctx);
    PTR(void) ram = _arg<1, PTR(void)>(rdram, ctx);
    u32 size = _arg<2, u32>(rdram, ctx);

    load_overlays(rom, ram, size);
}

// recomp_rom_read(offset): the ROM's big-endian word at that offset, as the game's ROM_READ reads it from the cartridge
// (patches/rom_reads.c).
extern "C" void recomp_rom_read(uint8_t* rdram, recomp_context* ctx) {
    u32 offset = _arg<0, u32>(rdram, ctx) & ~3u;
    std::span<const uint8_t> rom = recomp::get_rom();
    if (offset + 4 > rom.size()) {
        _return<u32>(ctx, 0);
        return;
    }
    _return<u32>(ctx, (u32(rom[offset]) << 24) | (u32(rom[offset + 1]) << 16) | (u32(rom[offset + 2]) << 8) |
                          u32(rom[offset + 3]));
}
