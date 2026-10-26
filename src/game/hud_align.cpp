// Widescreen's HUD: where a race's HUD is placed in a picture wider than 4:3. us.toml's hooks call fzx_hud_align at
// the start of each function that draws a part of it, with that function's display list pointer, and it writes
// RT64's gEXSetRectAlign there (rt64_extended_gbi.h), so the rectangles after it are placed from the picture's left or
// right edge, or as the game has them. RT64 places those from the edges of the HUD Placement the Graphics tab sets
// (Original keeps 4:3's), and with Aspect Ratio Original none of it moves anything.
//
// With three or four players each view is a quarter of the screen, whose sides aren't the picture's, so there every
// part is placed as the game has it.

#include "recomp.h"

namespace {
    constexpr uint32_t opcode_set_rect_align = (0x64u << 24) | 0x000006u; // RT64_EXTENDED_OPCODE, G_EX_SETRECTALIGN_V1
    constexpr uint32_t origin_left = 0x0;                                 // G_EX_ORIGIN_LEFT
    constexpr uint32_t origin_right = 0x400;                              // G_EX_ORIGIN_RIGHT
    constexpr uint32_t origin_none = 0x800;                               // G_EX_ORIGIN_NONE
    constexpr gpr num_players = (gpr)(int32_t)0x800CD000;                 // gNumPlayers
    constexpr int32_t screen_width = 320;
}

// side: 0 as the game places it, 1 from the left edge, 2 from the right. *gfx is the display list pointer, moved past
// the command.
extern "C" void fzx_hud_align(uint8_t* rdram, gpr* gfx, int side) {
    uint32_t origin = origin_none;
    int32_t offset = 0;
    if (MEM_W(0, num_players) <= 2) {
        if (side == 1) {
            origin = origin_left;
        } else if (side == 2) {
            // A rectangle's x is then from the right edge, so 4:3's right edge is subtracted (in quarter pixels).
            origin = origin_right;
            offset = -screen_width * 4;
        }
    }
    uint32_t offsets = ((uint32_t)offset & 0xFFFF) << 16;
    MEM_W(0, *gfx) = (int32_t)opcode_set_rect_align;
    MEM_W(4, *gfx) = (int32_t)(origin | (origin << 12));
    MEM_W(8, *gfx) = (int32_t)offsets;
    MEM_W(12, *gfx) = (int32_t)offsets;
    *gfx = (gpr)(int32_t)((uint32_t)*gfx + 16);
}
