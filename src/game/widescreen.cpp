// Widescreen's natives, called by us.toml's hooks: where a race's HUD is placed in a picture wider than 4:3, and its
// depth cleared to the picture's edges. The hooks call fzx_hud_align at the start of each function that draws a part
// of the HUD, with that function's display list pointer, and it writes
// RT64's gEXSetRectAlign there (rt64_extended_gbi.h), so the rectangles after it are placed from the picture's left or
// right edge, or as the game has them. RT64 places those from the edges of the HUD Placement the Graphics tab sets
// (Original keeps 4:3's), and with Aspect Ratio Original none of it moves anything.
//
// The game's scissor while it draws the HUD is 4:3's, so fzx_hud_scissor, at Hud_DrawHud's start, sets it again with
// its sides from the picture's edges (gEXSetScissor), as far in from them as the game's are from 4:3's. The game's next
// gDPSetScissor is placed as the game has it again.
//
// With three or four players each view is a quarter of the screen, whose sides aren't the picture's, so there every
// part is placed as the game has it.
//
// patches/widescreen.c's Camera_Draw opens a race's views of one or two players to the picture's edges, so the depth
// they draw against is cleared there too: fzx_race_clear_depth, a hook in Race_Draw just after the game's own depth
// clear (12, 16, 308, 224 with one player, 12, 8, 308, 232 with more), clears it again over the whole picture. It does
// so whatever the picture and the players, since the game's scissor keeps what it draws off the rest.

#include "recomp.h"

namespace {
    constexpr uint32_t opcode_set_scissor = (0x64u << 24) | 0x000005u;    // RT64_EXTENDED_OPCODE, G_EX_SETSCISSOR_V1
    constexpr uint32_t opcode_fill_rect = (0x64u << 24) | 0x000003u;      // RT64_EXTENDED_OPCODE, G_EX_FILLRECT_V1
    constexpr uint32_t opcode_set_rect_align = (0x64u << 24) | 0x000006u; // RT64_EXTENDED_OPCODE, G_EX_SETRECTALIGN_V1
    constexpr uint32_t origin_left = 0x0;                                 // G_EX_ORIGIN_LEFT
    constexpr uint32_t origin_right = 0x400;                              // G_EX_ORIGIN_RIGHT
    constexpr uint32_t origin_none = 0x800;                               // G_EX_ORIGIN_NONE
    constexpr gpr num_players = (gpr)(int32_t)0x800CD000;                 // gNumPlayers
    constexpr int32_t screen_width = 320;

    // Menus_Draw's scissor for the HUD: 12, 16, 308, 224 with one player, 12, 8, 308, 232 with two.
    constexpr int32_t scissor_side = 12;
}

// *gfx is Hud_DrawHud's display list pointer, moved past the command.
extern "C" void fzx_hud_scissor(uint8_t* rdram, gpr* gfx) {
    int32_t players = MEM_W(0, num_players);
    if (players > 2) {
        return;
    }
    // In quarter pixels; the right side is from the right edge.
    uint32_t left = scissor_side * 4;
    uint32_t right = (uint32_t)(-scissor_side * 4) & 0xFFFF;
    uint32_t top = ((players == 1) ? 16 : 8) * 4;
    uint32_t bottom = ((players == 1) ? 224 : 232) * 4;
    MEM_W(0, *gfx) = (int32_t)opcode_set_scissor;
    MEM_W(4, *gfx) = (int32_t)((origin_left << 2) | (origin_right << 14)); // G_SC_NON_INTERLACE is 0.
    MEM_W(8, *gfx) = (int32_t)((left << 16) | top);
    MEM_W(12, *gfx) = (int32_t)((right << 16) | bottom);
    *gfx = (gpr)(int32_t)((uint32_t)*gfx + 16);
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

// *gfx is Race_Draw's display list pointer, just past the game's depth clear (D_303A8F8 or D_303A810), moved past the
// commands. They are that list's, with the scissor and the rectangle from the picture's edges. The rectangle ends at the
// right edge itself, not a column before it: RT64 drops one whose lrx is less than its ulx before it places them, and
// a fill's right side is inclusive, so the scissor cuts it there.
extern "C" void fzx_race_clear_depth(uint8_t* rdram, gpr* gfx) {
    constexpr uint32_t depth_image = 0x003DBC00;                 // The lists' gsDPSetDepthImage
    const uint32_t words[] = {
        0xE7000000u, 0,                                                // gDPPipeSync
        0xE3000A01u, 0x00300000u,                                      // gDPSetCycleType(G_CYC_FILL)
        0xFF10013Fu, depth_image,                                      // gDPSetColorImage(RGBA, 16b, 320, depth)
        0xF7000000u, 0xFFFCFFFCu,                                      // gDPSetFillColor, the lists' depth
        opcode_set_scissor, (origin_left << 2) | (origin_right << 14), // gEXSetScissor(G_SC_NON_INTERLACE,
        0, 240 * 4,                                                    //   0, 0, 320, 240)
        opcode_fill_rect, origin_left | (origin_right << 12),          // gEXFillRectangle(
        0, 239 * 4,                                                    //   0, 0, 0 from the right, 239)
        0xE7000000u, 0,                                                // gDPPipeSync
        0xE3000A01u, 0x00000000u,                                      // gDPSetCycleType(G_CYC_1CYCLE), as they end
    };
    for (size_t i = 0; i < sizeof(words) / sizeof(words[0]); i++) {
        MEM_W(i * 4, *gfx) = (int32_t)words[i];
    }
    *gfx = (gpr)(int32_t)((uint32_t)*gfx + sizeof(words));
}
