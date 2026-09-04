# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

`AGENTS.md` in the repo root holds the longer-form conventions (style, sprite tooling details, commit/PR guidance). Read it before non-trivial changes; this file is the fast orientation.

## What this is

A Wolfenstein-style raycaster for the Psion 3a/3c/3mx (SIBO), written in C89 against the PLIB/WLIB SDK and built with the JPI/TopSpeed compiler (`tsc`). Target hardware is a ~27 MHz NEC V30MX with a 240x160 2-bit greyscale LCD. `PROGRAM.OPL` is the original OPL prototype, kept for reference only.

## Build

```
.\make.bat psion3d
```

`make.bat` calls `checkvid` then `tsc /m <project>.pr /smain=<name> /%jpivid%`, so the SIBO SDK tools must be on `PATH`. It uses `<name>.pr` if that file exists, otherwise `unnamed.pr`. Outputs `PSION3D.EXE` / `PSION3D.IMG` plus `*.OBJ` beside the sources (all gitignored).

**Adding a `.c` file requires adding a `#compile <name>` line to `unnamed.pr`** — the project file, not a wildcard, drives the build.

There is no automated test suite and no lint step. Verification means building, running `PSION3D.IMG` in an emulator or on device, and eyeballing rendering, movement, sprite occlusion, and map boundaries.

## Module map

| File | Owns |
| --- | --- |
| `psion3d.c` | `main()`, WLIB windows, keyboard scancodes, fixed-tick main loop, screen blit |
| `draw.c` | DDA ray cast, per-span wall depth buffer, sprite collection/sorting, player shot resolution |
| `walls.c` | Default wall style + `drawWall` function-pointer global |
| `labwall.c` | Lab environment wall style (`drawWallLab`) |
| `bitmap.c` | Local 1bpp screen buffers and fill/clear/pattern/line primitives |
| `videomem.a` | JPI assembler `blitVideoMem()` — direct writes to segment 0x40 video RAM |
| `sprite.c` | `.spr` loading into segments, frame cache, projection, drawing (`tst_spr.h` = fallback pattern) |
| `enemy.c` | Enemy state machine, AI tick, damage, per-type stats |
| `player.c` | Player position/movement, weapon table and firing state |
| `game_map.c` | 64x64 `map[][]`, ASCII-to-cell encoding, map file loading, per-level asset/wall-style selection |
| `fp_math.c` | 1024-entry combined sine/cosine table |
| `debug.c` | Debug text slots drawn into the left-hand 120x160 window |

## Architecture invariants

These are the things that break subtly if you get them wrong.

**Fixed point.** No runtime floating point. `f16` is Q8 (`256 == 1.0`, `fp_types.h`). Use `fpmul` (`s16*s16 -> s32 >> 8`) and the checked `fpdiv`; avoid division in hot loops. Trig input is Q8 radians; `fpsin`/`fpcos` index `sincos_tab` via multiply/shift masked with `TRIG_TABLE_MASK`. Keep that table at 32 columns per row.

**Two bitplanes in one buffer.** `screenBm` is a single 256x320 1bpp buffer: the top 256x160 half is the black plane, the bottom half is grey. `blackBm` and `greyBm` are pointers *into* `screenBm`, not separate allocations. Rows are 32 bytes wide, so byte addressing is `y << 5` and word addressing `y << 4`. Pixels are **low-bit-first**: pixel `x` uses `1 << (x & 7)`. Reversing that bit order swaps column pairs and produces jagged wall edges.

**Two blit paths.** `psion3d.c` defines `DIRECT_VIDEO_MEM_ACCESS`, which routes through the assembler `blitVideoMem()` (disables memory protection via port 0x15, `rep movsw` into segment 0x40, re-enables via port 0x14). The `#else` branch is the compatibility path: one `p_sgcopyto()` of the whole buffer to a segment-backed WLIB bitmap, then two `gCopyBit` calls. Keep both working. Outside of `videomem.a`, do not manipulate `DS`/`ES`, `cli`/`sti`, protection ports, or OS handle-table segments from C — past experiments with that crashed the emulator.

**Wall rendering boundary.** `draw.c` computes `wallHeight`, `f_wallDist`, `f_wallX`, `cell`, and `side` into a `wallhit_t`, then calls through the `drawWall` global. Environment-specific appearance, `WALL_TYPE_*` dispatch, doors, windows, and shading belong in a wall-style module (`walls.c`, `labwall.c`), selected per level in the `loadMapData()` `mapId` switch. A wall draw function returns `TRUE` when the column should write the wall depth buffer and `FALSE` for openings and non-occluding features — sprite occlusion and weapon impact resolution both depend on that contract.

**Column geometry.** Wall columns are 4 pixels wide, `x` aligned to a nibble boundary. Use `bmFillRect4` / `bmClearRect4` / `bmFillPattern4` for those; the general `bmFillRect` / `bmClearRect` / `bmFillPattern` are for variable-width sprites, UI, and 1-pixel detail marks.

**DDA corner case.** The ray loop in `draw.c` special-cases exact grid-corner crossings to avoid one-column wall gaps. Be careful changing `f_sidedx`/`f_sidedy`, `side`, or `mapx`/`mapy` stepping.

**Map cells are packed `u16`.** Flag bits (`MAP_MASK_SOLID`, `WALL`, `SPRITE`, `ENEMY`, `WALK`, `MARKED`) plus a 4-bit type in `MAP_BLOCK_TYPE_MASK` and a 6-bit id. `game_map.h` exposes `static` inline-style accessors (`mapCell`, `isWall`, `isSolid`, `canWalk`, …); out-of-range cells return a solid `WALL_TYPE_VOID` cell so callers never bounds-check. Map ASCII characters are decoded in `getCellEncoding()`; `C`/`E`/`F`/`G` delegate to `getEnemyCell()`.

**Timing.** 32 ticks per second (`units.h`). `runTicks()` catches up game state in whole ticks against `p_returntickcount()` before rendering once, so `updatePlayer()`/`runAI()` may run zero or many times per frame. `units.h` also carries the meters-per-second-to-map-units conversions (1 cell = 2 metres).

## Assets

Files are opened from full Psion paths: maps from `LOC::M:\IMG\MAP\map<N>.map`, sprites from `LOC::M:\IMG\SPR\<base><frame>.spr`. `p_read()` signals EOF with `E_FILE_EOF`, not `0` — treat any other short read as an error. Map loading writes straight into `map[][]`; do not add a second load buffer.

Sprites are 64x64, one frame per file, `base0.spr` … `base7.spr`; `loadSprite("sci", slot)` loads frame 0 then consecutive frames until one is missing, and sizes the segment to the frames actually loaded. Slot ids are in `sprslot.h`. A frame file is 1040 bytes — 16-byte header + 1024-byte row-major 2bpp payload; only the paragraph-aligned payload is copied into the segment. Pixel values: `0` transparent, `1` grey, `2` black, `3` white.

Convert PNGs with `tools\convert_sprite.bat`. For raw output use `.\tools\convert_sprite.bat /f tools\health.png health0.spr` or `-OutputPath` — do not use PowerShell redirection, it corrupts binary output. Pass `-WhiteTransparent true` only when near-white should become transparent (the default preserves it as white).

## Working style

Keep edits narrow. Do not reformat the generated trig table or other large tables unless asked, and do not delete the generated Psion artifacts (`*.OBJ`, `PSION3D.EXE`, `PSION3D.IMG`, `PSION3D.MAP`) unless explicitly asked to clean up. Follow the existing C style: tabs, braces on their own line, `static` helpers, project typedefs (`s16`, `u16`, `f16`, `s32`), and an `f_` prefix on fixed-point variables where scale matters.
