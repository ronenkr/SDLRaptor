# Milestone 4 — Polish

Status of the Raptor DOS → Windows/SDL3 port after M0–M3 (skeleton, video/timer,
input, audio). This file tracks what's left before the port is "done."

## Open bug (higher priority than the polish items below)

- **Silent exit ~10s after entering the hangar / starting a mission.** No
  exception is caught by the crash handler (`port/crash_win.c`) and nothing
  prints to stdout/stderr — consistent with heap corruption triggering a
  Windows fail-fast termination (`STATUS_HEAP_CORRUPTION`), which bypasses
  `SetUnhandledExceptionFilter` entirely. Reproduced via: New Mission → enter
  pilot name/callsign → Enter (auto-confirms difficulty) → crash while
  HANGAR_MUS is loading/playing.
  - Diagnostic traces were added to `audio/dmx_music.c` (`MUS_RegisterSong`/
    `MUS_PlaySong`/`MUS_FadeInSong`/`MUS_StopSong`/`MUS_UnregisterSong` all
    print to stderr) — **these are temporary and should be removed** once the
    root cause is found, or kept behind a `#ifdef RAPTOR_AUDIO_TRACE` if
    useful long-term.
  - Leading theory: a race or out-of-bounds array write in the AL_MIDI.C /
    MIDI.C path specific to HANGAR_MUS's instrument set (a channel/patch index
    RINTRO_MUS/MAINMENU_MUS don't exercise), or a lifetime bug where
    `g_mus_data` (audio/dmx_music.c) gets freed by `MUS_UnregisterSong` on the
    main thread while the audio thread is still mid-read of the old buffer
    inside `_MIDI_ServiceRoutine`. Worth auditing:
    - Whether `MIDI_StopSong()`/`TS_Terminate()` genuinely block until any
      in-flight audio-thread tick of `_MIDI_ServiceRoutine` has finished
      before `MUS_UnregisterSong` frees the buffer it was reading.
    - Bounds on `_MIDI_RerouteFunctions[channel]`, `ADLIB_TimbreBank[patch]`,
      `Voice[voice]` for whatever MIDI events HANGAR_MUS actually contains.
    - Consider running under Application Verifier / gflags page-heap to turn
      the silent corruption into an immediate, catchable access violation.

## Already fixed this session (context for the above)

- MUS tempo was playing at 2x speed: `mus2mid()`'s rate argument was
  hardcoded to 140 instead of the 70 Raptor actually passes to `DMX_Init`
  (`audio/dmx_sdl.c` now forwards it via `DmxMusic_SetRate`, see
  `audio/dmx_music.c`).
- `IMS_CheckAck`/`IMS_IsAck` (GFX/IMSAPI.C) never pumped SDL events, so any
  `while (IMS_IsAck());`-style busy-wait (used throughout WINDOWS.C/RAP.C/
  STORE.C/INTRO.C) could hang forever. Fixed by pumping (+ a small sleep) at
  that single choke point.
- `SFIELD` (GFX/SWDAPI.H) is walked directly over raw `.SWD` binary data with
  `curfld++`; the struct's trailing pointer was 4 bytes in the original
  32-bit build but 8 on x64, desyncing every field after the first in every
  window/dialog. Fixed by keeping that slot a 4-byte on-disk-compatible
  placeholder and moving the real pointer to a side table
  (`SWD_GetFieldSptr`/`SWD_SetFieldSptr` in GFX/swdapi.c).

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
