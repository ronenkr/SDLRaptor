# SDLRaptor
This is the original Raptor Call Of The Shadows DOS source code v1.2 (with the Apogee Sound System and the
DMX wrapper APODMX instead of the proprietary DMX library), ported to Windows on top of SDL3.

## Game data
The DOS release's data files are not part of this repository — only the empty `data/` directory is
tracked. Copy your own copy of the original game's files into it:

```
data/
  FILE0000.GLB
  FILE0001.GLB
  RAP.EXE
  RAPHELP.EXE
  SETUP.EXE
  ORDER.FRM
  VENDOR.DOC
  setup.ini
```

By default the port looks for this data in `C:/Projects/SDLRaptor/data`, set via the `RAPTOR_DATADIR`
CMake cache variable in [CMakeLists.txt](CMakeLists.txt). Pass `-DRAPTOR_DATADIR=<path>` to `cmake` if you
keep the data somewhere else.

## Building the Windows/SDL3 port
Requirements:
- MSVC (Visual Studio or the Visual Studio Build Tools) with the C++ workload, which also provides the
  bundled CMake and Ninja used by the build script.
- [SDL3](https://github.com/libsdl-org/SDL) development libraries, discoverable via CMake's normal
  `find_package(SDL3 CONFIG)` search (a system install on Linux is found automatically). On Windows,
  point CMake at an unpacked SDL3 devel package by setting the `SDL3_ROOT` environment variable before
  running the build script, or by passing `-DCMAKE_PREFIX_PATH=<path>` / `-DSDL3_DIR=<path>/cmake`
  directly to `cmake`.

Run [build.bat](build.bat) from the project root (targets the Visual Studio Build Tools):

```
build.bat            REM builds RelWithDebInfo into .\build
build.bat Debug      REM or pass a CMake build type
```

Or [build-vs2026.bat](build-vs2026.bat) if you have Visual Studio 2026 Community installed instead — same
usage and output.

This configures the project with Ninja into `build/` and compiles it; `build/raptor.exe` is produced
alongside the `SDL3.dll` it needs at runtime. `build/` is not tracked in git — delete it any time to force
a clean reconfigure.

## Command-line options
- `--data <dir>` — point at a game data directory other than the ones checked automatically (cwd, the exe's
  own directory, `RAPTOR_DATA` env var, then the `RAPTOR_DATADIR` build default).
- `--scaler <none|advmame2x>` — pixel-art upscaling filter for the 320x200 output, applied before the final
  stretch to the window. `advmame2x` (aka Scale2x) sharpens diagonal edges instead of blurring or staying
  blocky; `none` (default) is a straight nearest-neighbor stretch.
- `--newfeatures` — lets the Supplies shop sell more than one Auto Track Laser Turret (up to 3), Auto Track
  Mini Gun (up to 5), or Plasma Guns (uncapped). Owning more than one of a type makes it fire that many shots
  at once in flight, each aiming at a different on-screen enemy (sharing targets only once there are fewer
  enemies than shots). Off by default; normal one-of-each behavior is unchanged without this flag.

## Window and the shop screen
The game defaults to a 640x480 window, an exact 2x nearest-neighbor doubling of the original 320x200/240
picture (still resizable, and still governed by `--scaler` for the low-res picture itself). The "Supplies"
shop screen is the one exception: it's drawn natively at 640x480 (its own pixel buffer, not just a bigger
picture of the 320x200 one), showing every buyable/sellable item at once in a scrollable list instead of
the original one-item-at-a-time carousel, reusing the existing sprite/font assets doubled 2x.

## License
DOS Raptor is distributed under the GPL Version 2 or newer, see [LICENSE](https://github.com/skynettx/dosraptor/blob/master/LICENSE).

## Thanks
All my thanks go to [Scott Host](https://www.mking.com), [nukeykt](https://github.com/nukeykt) and [NY00123](https://github.com/NY00123) without them this release would not have been possible.
