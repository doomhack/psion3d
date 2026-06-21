# Repository Guidelines

## Project Structure & Module Organization

This is a Psion 3a/c/mx PLIB/WLIB C raycaster. Source is split by subsystem in the repository root:

- `psion3d.c` owns startup, windows, bitmaps, input, player position, and the main loop.
- `draw.c` owns ray casting, wall/sprite projection, and all wall drawing variants.
- `game_map.c` stores the 64x64 map; `game_map.h` exposes inline/static helpers for cell tests.
- `fp_math.c` stores the 1024-entry combined sine/cosine table; `fp_math.h` exposes lookup helpers.
- `fp_types.h` defines `u8/s16/s32`, Q8 fixed-point `f16`, `fpmul`, and checked `fpdiv`.
- `psion3d.h` shares renderer globals such as `gc`, `pos`, `dbgval`, and `gameWinRect`.

Generated outputs (`*.OBJ`, `*.MAP`, `PSION3D.EXE`, `PSION3D.IMG`) currently live beside source files.

## Build, Test, and Development Commands

- `.\make.bat psion3d`: builds with `tsc /m unnamed.pr /smain=psion3d`.
- `.\make.bat <name>`: uses `<name>.pr` if present, otherwise `unnamed.pr`.

The build requires the Psion/SIBO SDK tools (`checkvid`, `tsc`) on `PATH`. `unnamed.pr` lists modules with `#compile`; when adding a `.c` file, add it there and update link settings if the linker reports unresolved symbols.

There is no automated test suite. Verify by building, running the resulting `PSION3D.EXE` or `PSION3D.IMG` in the target emulator/device, and checking rendering, movement, sprite occlusion, and map boundary behavior.

## Coding Style & Naming Conventions

Follow the existing C style: tabs for indentation, braces on their own line, short local helpers marked `static`, and exported functions declared in headers. Use project typedefs when size matters (`s16`, `u16`, `f16`, `s32`). Fixed-point variables should use the `f_` prefix where it clarifies scale, such as `f_wallDist`.

## Performance & Architecture Notes

Target hardware is a 27 MHz NEC V30MX-class Psion. Avoid runtime floating point. Q8 fixed point is the default: `256 == 1.0`. Prefer `s16 * s16 -> s32` via `fpmul`; avoid division in hot loops unless unavoidable.

Trig input is Q8 radians. `fpsin`/`fpcos` convert to the 1024-entry `sincos_tab` using a multiply/shift and wrap with `TRIG_TABLE_MASK`. Keep `sincos_tab` formatted as 32 columns per row.

Rendering uses two bitplanes (`BM_BLK`, `BM_GRY`). `gClrRect`/`gFillPattern` calls often rely on the active GC, so preserve `gSetGC0` ordering carefully.

The DDA ray loop in `draw.c` handles exact grid-corner crossings specially to prevent one-column wall gaps. Be cautious when changing `f_sidedx/f_sidedy`, `side`, or `mapx/mapy` stepping.

## Commit & PR Guidelines

No strict project convention is established. Use imperative commit subjects, for example `Fix DDA corner hits` or `Split map storage`. PRs should note the build command used, emulator/device tested, and include screenshots for rendering changes.

## Agent-Specific Instructions

Keep edits narrow. Do not reformat large generated tables unless asked. Do not delete generated Psion artifacts unless the user explicitly requests cleanup.
