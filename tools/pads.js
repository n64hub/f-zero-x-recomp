// The pads plan's helpers (tools/play_plan_pads.json), an evalFile of N64Recomp-browser's tools/play_page.mjs played
// with its tools/gamepad_probe.js as the preload, whose stand-in pads (plugPads, padSet, padRumbles) these press.
//
//   mode()                          the screen that is up: gGameMode (0x800DCE44) & 0x1F, the decomp's GameMode
//                                   (1 GP race, 5 VS of four, 7 main menu, 8 machine select, 9 its settings)
//   padPress(pad, button, ms)       presses a button of the standard mapping (0 A, 9 Start, 12-15 the d-pad's up, down,
//                                   left and right) on stand-in pad <pad> for ms (150), as a thumb does
//   padUntil(pad, button, test, ms) presses it every 1.5 s until test() is true, for at most ms: a menu that ignores a
//                                   press while it animates, passed without timing the presses
//   cpuDrive(n)                     the computer drives racers 0 to n-1, the players' machines
//                                   (RACER_STATE_CPU_CONTROLLED in gRacers[i].stateFlags, 0x802C4924 + i * 0x3A8), so
//                                   a race goes on with nobody steering, and its machines meet the walls and each other
(() => {
  const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
  window.mode = () => n64r(0x800DCE44) & 0x1F;
  window.padPress = async (pad, button, ms = 150) => {
    padSet(pad, { buttons: { [button]: 1 } });
    await sleep(ms);
    padSet(pad);
  };
  window.padUntil = async (pad, button, test, ms) => {
    const end = performance.now() + ms;
    while (performance.now() < end) {
      if (test()) return true;
      await padPress(pad, button);
      await sleep(1350);
    }
    return test();
  };
  window.cpuDrive = (n) => {
    for (let i = 0; i < n; i++) {
      const a = 0x802C4924 + i * 0x3A8;
      n64w(a, n64r(a) | 0x800000);
    }
    return Array.from({ length: n }, (_, i) => (n64r(0x802C4924 + i * 0x3A8) & 0x800000) !== 0).every(Boolean);
  };
})();
