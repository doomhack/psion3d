# Native build

A development-only build of the game: the portable modules compiled with a
normal C compiler and hosted in a Qt window, so the renderer can be stepped
through in a debugger instead of an emulator. The Psion build is still the
shipping target and is untouched by anything in this directory.

## How it works

`fp_types.h` does `#include <plib.h>`, and every other header includes
`fp_types.h`, so the SIBO SDK reaches all twelve modules. The native build
simply leaves the SDK include directory off the compiler's search path and puts
`pc/compat/` on it instead, so `<plib.h>` and `<wlib.h>` resolve to the
replacements here. That is why no game source needed a `#ifdef`.

Only the parts of PLIB that are already a decent portable API are reimplemented
(`pc/src/plib_pc.c`): stream file I/O, the segment handle/offset/copy
interface, string helpers, and the tick counter. WLIB is not emulated at all —
its event queue *is* the device's frame trigger, and the Qt host replaces that
outright.

Excluded from this build: `psion3d.c` (windows, event pump, scancodes, blit),
`debug.c` (`gPrintText` and PLIB's software float package), `videomem.a` and
`fpasm.a` (JPI assembler). `pc/src/` supplies stand-ins for the last three.

## Build

Qt 6 is required. The build was set up against Qt 6.9.1 with the MinGW kit that
ships with Qt, so no other toolchain needs installing.

Two traps on this machine, both of which produce confusing failures:

- The `cmake` on `PATH` is devkitPro's MSYS2 build. It picks MSYS generators
  and mangles paths. Use `C:\Program Files\CMake\bin\cmake.exe`.
- MinGW's `bin` must be **ahead** of devkitPro's MSYS2 on `PATH`, or `gcc`
  fails with no error message at all and CMake reports only "is not able to
  compile a simple test program".

```bat
set PATH=C:\Qt\Tools\mingw1310_64\bin;C:\Program Files\CMake\bin;%PATH%
cmake -S pc -B pc/build -G "MinGW Makefiles" ^
      -DCMAKE_PREFIX_PATH=C:/Qt/6.9.1/mingw_64 ^
      -DCMAKE_C_COMPILER=C:/Qt/Tools/mingw1310_64/bin/gcc.exe ^
      -DCMAKE_CXX_COMPILER=C:/Qt/Tools/mingw1310_64/bin/g++.exe ^
      -DCMAKE_MAKE_PROGRAM=C:/Qt/Tools/mingw1310_64/bin/mingw32-make.exe ^
      -DCMAKE_BUILD_TYPE=Debug
cmake --build pc/build -j 8
```

## Run

Qt's DLLs must be findable:

```bat
set PATH=C:\Qt\6.9.1\mingw_64\bin;%PATH%
pc\build\psion3d_pc.exe
```

Arrows or WASD move, space fires, 1-4 select weapons, P pauses. Assets are read
from the working tree's `map/` and `spr/`, so an edited sprite shows up on the
next run without deploying anything.

Useful options:

| Option | |
| --- | --- |
| `--map <n>` | load a different level |
| `--assets <dir>` | read `map/` and `spr/` from somewhere else (or set `PSION3D_ASSETS`) |
| `--screenshot <file>` | render one frame to a PNG at 1:1 and exit, without opening a window |
| `--frames <n>` | advance *n* ticks on the virtual clock first; deterministic, so it pairs with `--screenshot` |
| `--gutter` | include backbuffer columns 240-255, which the LCD hides — anything drawn out there is a clipping bug |
| `--tick-start <n>` | seed the 16-bit tick counter, e.g. `65520`, to hit its wraparound in the first second rather than after 34 minutes |
| `-v` | report every failed file open, including `loadSprite`'s routine probe past each sprite's last frame |

## What this build is not for

**Its frame rate is meaningless.** The device renders uncapped at about 20fps
against 32 ticks a second; a PC does thousands. Pacing therefore defaults to
emulating the device rate so the game *feels* right, and the HUD says "PC fps"
so the number is never mistaken for a measurement. Renderer performance is
measured on hardware, by ablation — see `CLAUDE.md`.

The other thing to keep in mind is 16-bit arithmetic. `int` is 16 bits under
TopSpeed, so `u16 * u16` wraps there and does not here. The codebase is
disciplined about casting, and `INT`/`UINT`/`HANDLE` are typed 16-bit in
`pc/compat/plib.h` so SDK-typed variables behave at device width, but this is
still the one class of bug where "works on PC" can mislead. Configure with
`-DPSION3D_AUDIT_CONVERSIONS=ON` to have GCC flag the implicit narrowings.

## Layout

| | |
| --- | --- |
| `compat/plib.h`, `compat/wlib.h` | replacement SDK headers, pinned to the real declarations |
| `src/plib_pc.c` | file I/O, segments, ticks, string helpers |
| `src/pcclock.c` | the monotonic clock, kept apart because `<windows.h>` also typedefs `INT`/`UINT`/`BYTE`/`WORD`, at different widths |
| `src/fpasm_pc.c` | `fpmul`, plus the layout assertions `fpdiv` depends on |
| `src/debug_pc.c` | stands in for `debug.c` |
| `src/host.c` | the frame driver; the only PC file that includes game headers |
| `src/GameView.cpp` | plane unpack, palette, integer scaling, key handling |
| `src/MainWindow.cpp` | HUD, pacing and view menus |

No `.cpp` may include a game header: `fp_types.h` puns `fpsplit_t` through a
union, which is legal C but formally undefined in C++. `src/host.h` is the
entire surface the Qt layer sees, and it deliberately includes nothing.
