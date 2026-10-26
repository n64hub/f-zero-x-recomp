# The web page

The game plays in a browser: built with Emscripten on N64Recomp-browser's `frontend/` into `FZeroXRecompiled.html`
(the launcher, the recomp's menus, SDL2's audio and keyboard, the ROM from Load ROM, and the saves kept in IndexedDB;
`lib/N64Recomp-browser/frontend/CMakeLists.txt`'s header says what each part does), drawn through WebGPU. The page is
built from your ROM like the desktop program, so it is never built in CI and never published with the game in it.

What is the game's own stays here: the `EMSCRIPTEN` branches of `CMakeLists.txt` that make the game a page,
`tools/build_web.sh`, the prebuilt shader list (`src/rt64_shaders.txt`, recorded by `tools/shader_plan.json`; the
list's header says how to record it again) and the play plans (`tools/play_plan.json`, `tools/play_plan_extended.json`).
The rest is N64Recomp-browser's: `docs/porting.md` (WebGPU and the shaders, prebuilt shader lists, Emscripten),
`docs/playing.md` (playing and measuring a page), `docs/forks.md` and `docs/findings.md`. Every play of this game's
page is a line in N64Recomp-browser's `plays.jsonl` (game `fzx`), never a record here.

## Build it

On Linux with Docker, with the ROM at `roms/fzerox.us.z64`:

```bash
tools/build_web.sh                  # [cpus] [cmake arguments]
```

It builds `build/web/FZeroXRecompiled.html` with its `.js`, `.wasm` (about 12 MB) and `.data` (about 9 MB: the
recomp's fonts and menus, nothing of the game) in `tools/docker/Dockerfile.web`, the image every N64Recomp-browser page
is built in, with ccache in `build/ccache-web`, so a rebuild after a patch takes a minute or two. The script's header
says each step. Two things differ from the desktop build:

- The patches library (`patches/`, built for MIPS) is recompiled by the native N64Recomp that `tools/recompile.sh`
  builds, not the runtime's, which Emscripten would build for the browser.
- RT64 builds its pipelines from `src/rt64_shaders.txt` (`RT64_PREBUILT_SHADER_LIST`), in sections: the boot's before
  the game starts, and a course's the first time the course draws.

Serve `build/web` over HTTP from localhost (any static server; the page brings its own `coi_serviceworker.js` for the
headers threads need) and open `FZeroXRecompiled.html` in Chrome or Firefox with WebGPU, then Load ROM.

## The plans

A plan reaches a race the way a player does, by the keyboard's defaults (Enter is Start, Space A, WASD the stick):
Start at the title, then A through GP Race, machine select and the machine's settings. Two writes to RDRAM stand in
for what a script can't do well. The course is written at machine select (`gCourseIndex`, 0x800F8514), where the race
reads it as it starts, so a plan reaches any of the 24 courses without the cup and course menus. And once the race
starts, player 1's machine gets the computer's flag (`RACER_STATE_CPU_CONTROLLED` in `gRacers[0].stateFlags`,
0x802C4924), so it drives itself as the other 29 do and every play goes as far, whatever the browser. The plans' waits
and holds are on `gGameFrameCount` (0x800CCFE0), 60 a second, and `gGameMode` (0x800DCE44) says which screen is up.

`tools/play_plan.json` (about 2 minutes) boots, shoots the title and the main menu, machine select and its settings,
then plays a minute of the game's time on Mute City. `tools/play_plan_extended.json` (about 4 minutes) goes on from
the pause menu's Quit to the main menu and a minute on Rainbow Road. Play them with N64Recomp-browser's
`tools/play_page.mjs` (its `docs/playing.md`).

## The shader list

`src/rt64_shaders.txt` holds 75 shaders: 5 for the boot (the launcher, the title and the main menu), then a section a
course, the 24 of the Jack, Queen, King and Joker cups, 53-63 a section, most of them in every section. It was recorded
in headless Chrome at 1256x645 by `tools/record_shaders.sh`, a page load a course, about 30 minutes in all; its header
says what each section played and what it leaves out.

## How it plays

The regular plan on a desktop GPU at 1256x645, from the page as the port first built it (N64Recomp-browser's
`plays.jsonl`, game `fzx`): in Chrome, uncapped, 357.8 presents a second, held by the uncapped rate (360), no frame
gap over 50 ms, no sound short; in Firefox with its default prefs, 239.9, the display's rate, one gap of 114 ms.
Neither missed a shader or had a WebGPU error or exception. Chrome builds the whole list behind the launcher; Firefox builds each
section as it is reached and waits 2.2 s for Mute City's as the race starts. The game itself runs a one-player race at
60 frames a second, as on a console, and RT64 interpolates the rest.
