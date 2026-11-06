# How F-Zero X: Recompiled fits together

The facts about the game and its recompilation that the code can't say on its own. CLAUDE.md points here; a
finding that explains the game's code goes here or beside that code.

- **The code.** `main` is stored raw in the ROM (MIO0 holds only the courses' textures and models) and the runtime
  maps what lies in ROM 0x1000 plus 1 MB at start. Every other code section is an overlay the game loads through
  `Dma_LoadOverlay` (src/sys/dma.c), which `patches/overlays.c` patches to tell the runtime: `ovl_i2` (ROM 0x98080,
  VRAM 0x800FC730) and `ovl_i10` at boot, and the menus', the race's and the ending's, which share VRAM
  0x80115DF0, each time the screen changes.
- **The entry point** is the decomp's at 0x80067000; the ROM's header says 0x80267000, and the CIC-6106 boot code
  jumps 0x200000 below it.
- **Reads of the ROM by the CPU.** The game reads ROM words straight through KSEG1 (`ROM_READ`) and waits on the PI's
  status register in three functions: `Racer_InitMachineStats` (the machines' physics constants come from the boot
  code's words), `Machine_DrawBlueFalconLod1` and `func_i6_8011DBD0` (both checks of a ROM word). The runtime maps no
  cartridge, so `patches/rom_reads.c` has those three read through `recomp_rom_read` (`src/game/recomp_api.cpp`) and
  leaves out the waits.
- **No 64DD.** The game asks at boot whether one is attached (`LeoDD_CheckPresence`); `patches/leo.c` answers no, as
  a console without one does. `us.toml` stubs the 64DD's boot of a disk game, and `src/game/libultra.cpp` has the
  64DD's and the other libultra functions N64Recomp leaves out that the game's code links in.
- **Microcodes.** Graphics: F3DEX2 (fifo, and its Rej), F3DLX2.Rej, L3DEX2 and the game's own F3DFLX2.Rej, which RT64
  knows by its hash but typed Unknown, so its lists ran as RDP commands. `runtime-patches/rt64-0001` draws it as
  F3DEX2 plus its two differences, as GLideN64 has them: MOVEMEM's light 1 is an alpha light, and its texture gen
  sets no texture coordinates but each vertex's alpha, from the 256-byte table G_DMA_IO points at, by the normal's
  angle to that light. The machines' paint and shading come from that alpha: without it they draw flat. Audio:
  aspMain, recompiled by RSPRecomp from `aspMain.us.toml` (its header says how its layout was found). librecomp's
  vector unit uses SSE4.1, so `RecompiledFuncs` builds with `-march=nehalem` on x86-64.
- **Jump tables.** N64Recomp finds all of the game's itself, so `us.toml` lists none by hand.
- **Saves.** SRAM, 32 KB. `main.cpp` declares `SaveType::Sram`; the runtime keeps it in `saves/fzerox.n64.us.1.0.bin`
  and writes it a moment after the game saves, so a test waits a few seconds before it stops the game.
- **Input.** RecompFrontend's default bindings, nothing of the game's own. Keyboard: WASD the stick, Space A, Left
  Shift B, Q Z, R R, E L, Enter Start, the arrows the C buttons. A test that posts `WM_KEYDOWN`/`WM_KEYUP` to the
  window can't hold Left Shift (SDL2 on Windows releases it at every pump when `GetKeyState` says it is up), so it
  gives B a second key in `controls.json`.
- **Testing without the speakers.** `main.cpp` asks SDL for WASAPI only when `SDL_AUDIODRIVER` isn't set, so a test
  records the game's sound with SDL's disk driver (`SDL_AUDIODRIVER=disk`, `SDL_DISKAUDIOFILE=<file>`: F32 stereo at
  48 kHz).
- **Interpolation.** `patches/interpolation.c` rewrites each frame's finished list (Gfx_FullSync) for RT64: a camera's
  combined projection and view becomes the two, from the camera, since its off-centre frustum defeats RT64's split, and
  the racers' matrices and boosters get ids by racer. Its header says how. RT64 blends a camera's eye on the line
  between its two places (`RT64_INTERPOLATE_CAMERA_EYE`, on in `CMakeLists.txt` for the desktop build as the page has
  it): F-Zero X's cameras are far from the origin and turn fast, and blending the view's translation swung the
  machine in front of the camera in the frames between.
- **Widescreen.** RecompFrontend's Graphics tab defaults to Aspect Ratio Expand, HUD Placement 16:9, Resolution Auto
  and Framerate Display. RT64 widens the 3D; the rest is the game's to say. `patches/widescreen.c` widens the race's
  sky, floor and clouds, which the game sizes to a 4:3 view, by the window's aspect ratio (`recomp_get_aspect_ratio`,
  4:3 with Aspect Ratio Original). The game draws a black border round a race, a console's too: its scissor keeps the
  3D in from 4:3's edges (12, 16, 308, 224 with one player, 12-307 for the halves of two), and RT64 keeps that as far in
  from the window's. So `widescreen.c`'s `Camera_Draw`, which sets every race view's scissor, opens one or two
  players' views to the picture's edges (`gEXSetScissor`) in a picture wider than 4:3. The game clears the depth
  only inside its scissor, and a second clear over the whole picture cost a quarter of the presents at 4K and set off
  a crash in RT64's render to RAM on the web, so there is none. The one-player view's viewport is the whole 320x240
  (`aVpFullScreen`, in the MIO0 of `course_track_gfx`); the top half's starts 4 lines down, so its scissor does too.
  The other screens (the title, the menus, Records, the GP's ending) draw in the game's 12, 8, 308, 232 frame, and its
  patched `func_80069698` enlarges that to the whole picture after each one draws, before the transition, whose
  background is the frame: the framebuffer loaded as a texture a few rows at a time, which RT64 takes from what it drew.
  Three or four players' quarters keep the game's border, and each screen's first three frames are cleared to black so
  the last one's picture isn't left beside them. The race's HUD is placed by N64Recomp hooks (`us.toml`, `[[patches.hook]]`) at the start of each
  function that draws a part of it, which write RT64's `gEXSetRectAlign` into its display list
  (`src/game/hud_align.cpp`): the portraits, lap and spare machines from the left edge, energy, time, the minimap and
  the speed from the right, the rank and messages centred. A hook at `Hud_DrawHud`'s start sets the game's 4:3 HUD
  scissor again from the edges, which would cut those off; the game's next `gDPSetScissor` puts it back. Three or four
  players' views are quarters, so their HUD stays as the game places it. A hook's `before_vram` is the function's
  first instruction, or one inside it whose registers it names (`Menus_Draw`'s list is in `$s3`).
- **The game's own pacing**: its main thread wakes at every VI (`osViSetEvent` with a count of 1), and a race with
  one player runs at 60 frames a second on a console.
