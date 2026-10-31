# F-Zero X: Recompiled

> **Made with AI.** Almost all of the code, documentation and tests in this repo were written by AI agents
> (Claude), directed and play-tested by a person. None of it is offered to the upstream projects it builds on, which
> don't take AI-written code.

A native PC port of the N64 *F-Zero X* (USA), made by static recompilation
([N64Recomp](https://github.com/N64Recomp/N64Recomp) + [N64ModernRuntime](https://github.com/N64Recomp/N64ModernRuntime) +
[RT64](https://github.com/rt64/rt64), with [RecompFrontend](https://github.com/N64Recomp/RecompFrontend)'s launcher
and menus): the game's own MIPS code translated to C and run natively, with RT64 drawing it. The symbols come from the
[inspectredc/fzerox](https://github.com/inspectredc/fzerox) decompilation. It is not an emulator.

The repo holds no ROM, no asset from the game and no data extracted from it: you supply your own ROM, and nothing in
git or CI is made from one except the symbol names and addresses in `syms/`.

**Status:** the desktop build boots to the title, plays the attract demo's races (split screen too) and goes through
the menus into a Grand Prix race, with sound (the audio microcode recompiled), SRAM saves and keyboard and gamepad
input. The game's own graphics microcode, F3DFLX2, is drawn with its alpha light, which gives the machines their paint
and shading (`runtime-patches/`). Races interpolate to the display's rate (`patches/interpolation.c`): each camera,
machine, shadow and booster glow is blended between two game frames by who it is. In a window wider than 4:3 a race
of one or two players draws to the window's edges, sky included, without the black border the game draws on a console
(three or four players' quarters keep it), the title's and the menus' framed screens are enlarged to fill it, the
transitions between screens cover all of it, and a race's HUD sits at the picture's sides (Graphics: Aspect Ratio
Expand and HUD Placement 16:9 by default, Original for the console's 4:3 and its border). The game also plays in a browser, built from your ROM as a web page
([docs/web.md](docs/web.md)).

Up to four people race each other in VS Battle, each with a pad of their own or the keyboard: each pad plays a player of
its own with no setup, in the order the pads were plugged in, and the keyboard plays player 1 beside the first pad. To
choose who plays which, open the menu (Esc), and on Controls press Assign players, then a button on each player's pad
(or a key) in the players' order, and Confirm; after, the keyboard stays player 1's unless a player was given it. The
game counts its controllers as it starts, as on a console, so VS Battle offers as many players as there were pads then
(or players assigned): plug them in before Start Game.

## The ROM

*F-Zero X* (USA), the first release (`CFZE`), as a big-endian `.z64` of 16 MiB with SHA-1
`5f658e88ffa9de23cba6986a8fd3d3a90d7b4340`. Other regions, revisions and the 64DD's Expansion Kit are not supported.

## Getting it

Releases for Windows (a zip: unzip it and run `FZeroXRecompiled.exe` from that folder) and Linux on x86-64 (a
`.tar.gz`: extract it and run `FZeroXRecompiled` in that folder) are on the
[Releases page](https://github.com/n64hub/f-zero-x-recomp/releases), without the ROM: pick yours with Load ROM.
The Linux build needs glibc 2.35 or newer (Ubuntu 22.04, Debian 12 or later), a Vulkan driver, and GTK 3 and FreeType,
which desktop distributions have; it brings its own SDL2. Or build it as below.

## Building

You need that ROM at `roms/fzerox.us.z64`. On Windows: LLVM's clang-cl, CMake 3.31 and Ninja, and a clang with a MIPS
backend plus ld.lld for `patches/`. On Linux: `clang lld libsdl2-dev libgtk-3-dev libfreetype-dev`, CMake 3.31 and
Ninja.

From the repo's root in a bash shell:

```sh
git submodule update --init --recursive
bash tools/recompile.sh -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl   # the game's code as C
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl \
      -DPATCHES_C_COMPILER=<MIPS clang> -DPATCHES_LD=<ld.lld>
ninja -C build FZeroXRecompiled
```

`tools/recompile.sh` builds N64Recomp from the submodule the first time, then translates the game. Run
`build/FZeroXRecompiled.exe` from the repo's root, pick the ROM with Load ROM, then Start Game.

The symbols in `syms/` are made from the decomp's own build of the game (`tools/build_decomp.sh`, which needs the ROM
and Docker, then `tools/gen_syms.sh`). They only change when `lib/fzerox` moves.

A release is built by `tools/release.sh`: `windows` (Git Bash) and `linux` (Linux with Docker) build and pack its
two archives into `out/` from the same commit of `main`, and `publish` publishes them as the GitHub release
`v<version_string>` (`src/main/main.cpp`) with `releases/v<version>.md` as its notes (the script's header says more).

## License

GPL-3.0 (`LICENSE`), as the recompilations it follows. The decomp in `lib/fzerox` is its own project, under CC0.
