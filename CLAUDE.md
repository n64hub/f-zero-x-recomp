# CLAUDE.md

A static recompilation of the N64 *F-Zero X* (USA) with N64Recomp, N64ModernRuntime, RecompFrontend and RT64, from
the inspectredc/fzerox symbols. Almost all of it is written by AI agents, so this file is what the next one needs: the
rules, where things are, and the facts about the game the code can't say on its own.

## What lives where

This repo is the game: its recompilation, the patches over it, the desktop build and its web page.

- `syms/`: the symbols N64Recomp reads, generated (never edited by hand) by `tools/build_decomp.sh` then
  `tools/gen_syms.sh` (`tools/gen_syms.py` says how).
- `us.toml`: N64Recomp's configuration; `tools/recompile.sh` writes `RecompiledFuncs/` with it, and the audio
  microcode with `aspMain.us.toml`.
- `patches/` and `patches.toml`: MIPS C against `lib/fzerox`'s headers, recompiled into PatchesLib. Its `ui_funcs.h`,
  `patch_helpers.h` and `recompui_event_structs.h` are RecompFrontend's, which includes the first from the game's
  `patches/`.
- `src/`: RecompFrontend's main (`src/main/main.cpp`, with the GameEntry), the natives the patches call
  (`src/game/recomp_api.cpp`), the libultra functions the runtime lacks (`src/game/libultra.cpp`) and the settings.
- `lib/`: `fzerox` (the decomp, for the symbols and the patches' headers), upstream `rt64`, `N64ModernRuntime` and
  `RecompFrontend` at the commits N64Recomp-browser's web forks are built on, and `N64Recomp-browser` at a release tag.
  The web support every recomp's page shares is developed and documented there; `tools/build_n64recomp.sh` and
  `CMakeLists.txt` apply its `patches/` to the other three, and `CMakeLists.txt` then this game's own changes to them
  (`runtime-patches/`, its README lists them).
- Comments beside the code, for how and why: each file's header. Read the header of the file you change and update
  it in the same commit.

## Rules

- No ROMs, ROM-derived assets or extracted game data in git, CI or PRs (`tools/ci/check_repo.py` checks). Nothing
  from the decomp's `assets/` or its build is committed but `syms/`. The launcher's art is text only (`icons/`): no
  logo, character or picture from the game.
- The decomp is built only where the ROM is, never in CI.
- The scope is the game as it plays, plus what a web page needs (RT64 interpolation to the display's rate, resolution
  Auto), plus modern enhancements: each an option with an Original value that plays as the console does, none
  changing how the game plays. USA only: the Expansion Kit is out of scope.
- Code from G-Diffuser or any other F-Zero X project may be read for what it changes, but nothing that isn't the
  decomp's is copied.
- Write for a stranger, in the present tense: nothing about a contributor's machine, servers, paths or accounts, and
  nothing about where anyone keeps ROMs.
- GPL-3.0, as the recompilations it follows.

## How it fits together

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
  the racers' matrices and boosters get ids by racer. Its header says how. RT64 blends a camera's own transform, not
  the view (`runtime-patches/rt64-0002`): F-Zero X's cameras are far from the origin and turn fast.
- **The game's own pacing**: its main thread wakes at every VI (`osViSetEvent` with a count of 1), and a race with
  one player runs at 60 frames a second on a console.
