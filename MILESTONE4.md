# Milestone 4 — Polish

Status of the Raptor DOS → Windows/SDL3 port after M0–M3 (skeleton, video/timer,
input, audio). This file tracks what's left before the port is "done."

## Remaining M4 scope

1. **Gamepad/joystick.** `port/ptr_blit.c`'s `PTR_ReadJoyStick` is currently a
   stub (returns centered/no buttons). Wire it to `SDL_Gamepad` axes/buttons,
   feeding `joy_x`/`joy_y`/`joy_buttons` the way the original gameport read
   did. Also drives `IPT_GetButtons`'s `inp(0x201)` emulation in
   `port/compat` for `INPUT.C`'s joystick branch.
2. **Fullscreen toggle.** Alt+Enter → `SDL_SetWindowFullscreen`. Needs a key
   handler hook in `port/plat_pump.c` (Alt+Enter isn't a game key, shouldn't
   reach `KBD_HandleEvent`).
3. **Save/load verification.** Exercise `RAP_FFSaveFile`/load-game path
   end-to-end once the hangar crash above is fixed (save games write to the
   data dir as `CHAR%04u.FIL`; confirm round-trip works from a fresh
   pilot through a full session).
4. **Demo record/playback.** `RAP.EXE REC <file>` / `PLAY <file>` args are
   forwarded by `port/plat_main.c` but untested. Known risk: MSVC's `rand()`
   won't match Watcom's sequence, so recorded-demo enemy RNG may diverge from
   the original DOS demos (see plan doc's accepted-risk note).
5. **`/W4` warnings sweep.** Current build is warning-tolerant (only fixed
   the ones that were genuine x64 bugs — see plan doc §8). Do a pass with
   `/W4` across the `port/` and `audio/` sources specifically (the ported
   original game/audiolib code is expected to be noisy and lower-value to
   clean up).
6. **Window/taskbar polish.** App icon, `VS_DEBUGGER_WORKING_DIRECTORY` is
   already set in CMakeLists.txt; confirm it still points at the right data
   dir after the SDLRaptor rename.

## Verification checklist for M4 sign-off

- [ ] Fresh pilot → hangar → buy something in the store → start wave 1 →
      complete it → back to hangar → save → quit → relaunch → load → confirms
      the hangar crash is gone and save/load round-trips.
- [ ] Gamepad moves the ship and fires in a mission.
- [ ] Alt+Enter toggles fullscreen and back without breaking rendering.
- [ ] `raptor.exe REC test.dem` then `raptor.exe PLAY test.dem` plays back
      without crashing (visual divergence from RNG mismatch is acceptable).
- [ ] Clean `/W4` build log reviewed for the `port/`+`audio/` sources.
