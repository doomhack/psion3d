# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

`AGENTS.md` in the repo root holds the longer-form conventions (style, sprite tooling details, commit/PR guidance). Read it before non-trivial changes; this file is the fast orientation.

`TASKS.md` is the project task list - planned features and their current state. Check it before starting new work, and tick items off there as they land.

`MEMORY_BUDGET.md` is the measured memory breakdown - near data (DGROUP) is the binding limit at 67.1% of 64 KB (5.0 KB to memcheck's 48 KB limit), not the 512 KB system total, and near code is at 74.3% and the one that grows. Check it before adding a global array, and run `.\tools\memcheck.bat` after the build to see the numbers and the delta (`-Baseline old.MAP` names the regions that moved; `-Record "note"` appends the history row).

## What this is

A Wolfenstein-style raycaster for the Psion 3a/3c/3mx (SIBO), written in C89 against the PLIB/WLIB SDK and built with the JPI/TopSpeed compiler (`tsc`). Target hardware is a ~27 MHz NEC V30MX with a 240x160 2-bit greyscale LCD. `PROGRAM.OPL` is the original OPL prototype, kept for reference only.

## Build

```
.\make.bat psion3d
```

`make.bat` calls `checkvid` then `tsc /m <project>.pr /smain=<name> /%jpivid%`, so the SIBO SDK tools must be on `PATH`. It uses `<name>.pr` if that file exists, otherwise `unnamed.pr`. Outputs `PSION3D.EXE` / `PSION3D.IMG` plus `*.OBJ` beside the sources (all gitignored).

**Adding a `.c` file requires adding a `#compile <name>` line to `unnamed.pr`** — the project file, not a wildcard, drives the build. If the file is portable, add it to `GAME_SOURCES` in `pc/CMakeLists.txt` too; both lists are explicit, and the CMake one must never be a glob or it will sweep `psion3d.c` back in.

There is also a native development build — see `pc/README.md`. It compiles the portable modules against replacement SDK headers and hosts them in a Qt window, so the renderer can be debugged with breakpoints instead of an emulator. It does not replace on-device testing, and its frame rate means nothing: performance is still measured on hardware.

There is no unit-test suite and no lint step. **After any code change, run `.\tools\verify.bat`** (the `/verify` skill explains the output): it does the DOSBox build, hashes `PSION3D.IMG` against the previous build, runs the DGROUP check with per-region deltas, builds the PC host and diffs the golden frames in `golden/` — about 15 seconds. A refactor must leave the image unchanged and every frame matching; a rendering change should be looked at in `.verify\frames\` and then accepted with `-UpdateGolden`. What it cannot check - feel, sprite occlusion in motion, performance - still means running `PSION3D.IMG` in an emulator or on device.

**The toolchain is 16-bit DOS**, so it runs under DOSBox (installed at `C:\Program Files (x86)\DOSBox-0.74-3`). Its configured mounts are `C:` = `E:\dosroot` (SIBOSDK + TopSpeed) and `D:` = this repo, so a headless build is:

```
"C:\Program Files (x86)\DOSBox-0.74-3\DOSBox.exe" -c "D:" -c "tsc /m unnamed.pr /smain=psion3d /v0 /zq > D:\build.log" -c "exit"
```

That takes about 5 seconds. Three things matter:

- `/zq` (quiet mode) makes `tsc` write to stdout, so warnings and errors can be redirected to a file. Without it the output goes straight to video memory and the log is empty.
- The trailing `-c "exit"` is what closes DOSBox. To go through `make.bat` instead of `tsc`, use `-c "call make.bat psion3d"` - without `call` the batch never returns, the `exit` never runs, and DOSBox hangs forever.
- Do not pass `-c "config -set cycles max"`; the conf already sets `cycles=max` and the override stalls the build.

On an error `tsc` deletes the offending `.OBJ` and `PSION3D.EXE`, so an unchanged `PSION3D.IMG` timestamp is a second failure signal.

To check a refactor changed no code, hash `PSION3D.IMG`, **not** `PSION3D.EXE`. The `.EXE` embeds a build timestamp at offset 30-33, so two builds of identical sources differ in those four bytes and nothing else. The `.IMG` is byte-reproducible and is the deployed artifact. Running the result still needs an emulator or the device. Verify logic by simulation where you can — PowerShell works well for fixed-point and blitter maths: implement the old and new versions and compare bit patterns over the full input range. Confirm the new path actually executed in the simulation; a test that silently skips the new branch proves nothing.

Working-tree line endings are mixed (git stores LF, some files are CRLF on disk) and the toolchain accepts both. Use `git diff --ignore-cr-at-eol` or the noise buries real changes, and check bytes with PowerShell rather than the bash tool, which translates line endings on read.

## Module map

| File | Owns |
| --- | --- |
| `psion3d.c` | `main()`, WLIB windows, keyboard scancodes, key events, fixed-tick main loop, screen blit |
| `gameloop.c` | The mode above the frame (`gameMode`: menu or playing), `gameInit`/`gameKey`/`gameStartMission`, and the tick catch-up loop both platforms call |
| `menu.c` | The menu screens (main, mission select, briefing, objectives, pause, abort, pause objectives, map): state, keys and drawing through `ui.h` |
| `ui.h` / `ui_psion.c` | The drawing seam the menus use, and its WLIB implementation: the 480x160 menu and HUD windows (`uiTarget`), ROM Swiss 13/16 fonts. `pc/src/menu_pc.cpp` is the QPainter one |
| `mission.c` | Mission index (titles/locations parsed from `map1..map20.map` at startup into a far segment), `difficulty`, `objectiveState[]` |
| `automap.c` | The pause menu's level plan, rendered into `screenBm` with `bitmap.c` and copied out with `uiBlitMap` |
| `settings.c` | The Options screen's values (`soundLevel`, `showFps`); process lifetime, no save file yet |
| `cheat.c` | Cheats (task 24): `cheatFlags` from the Cheats screen, `cheatActive` for the mission in play (none on the benchmark map), names, texts, radio groups; the hooks themselves sit in player, enemy, draw, sprite, automap and gameloop, all testing `cheatActive` |
| `bench.c` | The benchmark: the stations of `map97.map` in turn, world frozen, fps per station for the `MENU_BENCH` results screen; under `BENCH_PROFILE` (`tools\profile.bat`) the ablation passes and per-part costs instead |
| `hud.c` | The in-game HUD panels (health, objectives, weapons/ammo, fps) drawn through `ui.h` into the HUD target: static parts on a redraw event, values as single-call cells replaced only when they change (never invalidate the HUD window for a value; see task 8) |
| `draw.c` | DDA ray cast, per-span wall depth buffer, sprite collection (in ray order, no depth sort), player shot resolution |
| `walls.c` | Default wall style + `drawWall` function-pointer global |
| `labwall.c` | Lab environment wall style (`drawWallLab`) |
| `bitmap.c` | Local 1bpp screen buffers and fill/clear/pattern/line primitives |
| `videomem.a` | JPI assembler `blitVideoMem()` — direct writes to segment 0x40 video RAM |
| `fpasm.a` | JPI assembler `fpmul()` — the Q8 multiply, using the V30's native `IMUL` |
| `ddaasm.a` | JPI assembler `ddaWalk0`..`ddaWalk3` — the DDA step loop, one entry per quadrant (picked once per ray through `ddaWalkers[]` in `draw.c`), all six of its values in registers, a pointer walk through `map[][]`; `pc/src/ddaasm_pc.c` is the C reference |
| `bmasm.a` | JPI assembler `bmClearScreen()` (one `rep stosw` over both planes) and the wall styles' `bmFillRect4` / `bmClearRect4` / `bmFillPattern4` (clipping in registers, then a jump into 160 unrolled rows); `pc/src/bmasm_pc.c` is the C twin |
| `sprasm.a` | JPI assembler `spriteDrawRows()` — a scaled sprite's row loop, set up by `drawProjectedSprite` in one global block (`spriteRows`): per row the Y interpolant and span byte, per pixel the X interpolant and two `shr`/`rcr` pairs into the plane bits, all in registers; `pc/src/sprasm_pc.c` is the C twin |
| `sprite.c` | `.spr` loading into segments, frame cache, projection, drawing. A slot that failed to load draws nothing; there is no built-in fallback sprite. [SPRITES.md](SPRITES.md) explains the whole pipeline |
| `enemy.c` | Enemy state machine, AI tick, damage, per-type stats |
| `player.c` | Player position/movement, weapon table and firing state |
| `game_map.c` | 64x64 `map[][]`, ASCII-to-cell encoding, map file loading, per-level asset/wall-style selection |
| `fp_math.c` | 1024-entry combined sine/cosine table |
| `debug.c` | Debug text slot (`setDbg*`); the device window that drew it went with the HUD, the PC HUD label still shows it |

## Architecture invariants

These are the things that break subtly if you get them wrong.

**Fixed point.** No runtime floating point. `f16` is Q8 (`256 == 1.0`, `fp_types.h`). `fpmul` is hand-written assembler in `fpasm.a` using the V30's native `IMUL`; C cannot reach that instruction, because `(s32)a * b` promotes both operands and TopSpeed emits its generic 32x32 helper `N$SgnMol` instead. Its calling convention is declared with a `#pragma call` on the prototype — keep the pragma and the assembler in step. `fpdiv` is still C and still calls the bit-serial `N$SgnDiv`, so keep it out of hot paths; note that `fpdiv(a, int2fp(n))` is just `a / n`, because the Q8 shifts cancel. Trig input is Q8 radians; `fpsin`/`fpcos` index `sincos_tab` via `trigidx`, which routes through `fpmul`. Keep that table at 32 columns per row.

To find remaining 32-bit arithmetic, ask the object files rather than reading code:

```bash
grep -aoi "N.\{0,1\}\(SgnMol\|SgnDiv\|LngShr\|LngShl\)" *.OBJ | sort | uniq -c
```

**Assembler modules.** `.a` files are JPI/TopSpeed assembler, listed in `unnamed.pr` alongside the C modules. The syntax has traps: `;` comments are a **syntax error**, and displacements concatenate brackets — `mov es:[di][9600],al`, not `[di+9600]`. Arguments arrive in `AX`, `BX`; declare the convention explicitly with `#pragma call(reg_param=>(...), reg_saved=>(...))` on the C prototype rather than relying on the default. Two more traps, met writing a third module: the file must have **CRLF line endings** or every line is a syntax error reported on line 1, and `jmp` and `call` are short unless written `jmp near` / `call near`, so a jump over more than 127 bytes needs `near` (a conditional jump cannot be near - invert it and `jmp near` past). `[bp][n]` addresses the stack (SS), so keep locals there and data behind `[si]`, `[di]`, `[bx][si]`, `[bx][di]`; more than two arguments go in a block of two-byte members whose address arrives in `AX`.

**Two bitplanes in one buffer.** `screenBm` is a single 256x320 1bpp buffer: the top 256x160 half is the black plane, the bottom half is grey. `blackBm` and `greyBm` are pointers *into* `screenBm`, not separate allocations. Rows are 32 bytes wide, so byte addressing is `y << 5` and word addressing `y << 4`. Pixels are **low-bit-first**: pixel `x` uses `1 << (x & 7)`. Reversing that bit order swaps column pairs and produces jagged wall edges.

**The black plane sits on top of the grey plane.** A pixel set in both reads as black, so the display shows three shades — background, grey, black — not four. Most black on screen arrives this way rather than from the black plane alone, so treating "both set" as a separate darker shade visibly mis-renders the majority of it.

**Two blit paths.** `psion3d.c` defines `DIRECT_VIDEO_MEM_ACCESS`, which routes through the assembler `blitVideoMem()` (disables memory protection via port 0x15, `rep movsw` into segment 0x40, re-enables via port 0x14). The `#else` branch is the compatibility path: one `p_sgcopyto()` of the whole buffer to a segment-backed WLIB bitmap, then two `gCopyBit` calls. Keep both working. Outside of `videomem.a`, do not manipulate `DS`/`ES`, `cli`/`sti`, protection ports, or OS handle-table segments from C — past experiments with that crashed the emulator.

**Wall rendering boundary.** `draw.c` computes `wallHeight`, `f_wallDist`, `f_wallX`, `cell`, and `side` into a `wallhit_t`, then calls through the `drawWall` global. Environment-specific appearance, `WALL_TYPE_*` dispatch, doors, windows, and shading belong in a wall-style module (`walls.c`, `labwall.c`), selected per level in the `loadMapData()` `mapId` switch. A wall draw function returns `TRUE` when the column should write the wall depth buffer and `FALSE` for openings and non-occluding features — sprite occlusion and weapon impact resolution both depend on that contract.

**Column geometry.** Wall columns are 4 pixels wide, `x` aligned to a nibble boundary. Use `bmFillRect4` / `bmClearRect4` / `bmFillPattern4` for those; the general `bmFillRect` / `bmClearRect` / `bmFillPattern` are for variable-width sprites, UI, and 1-pixel detail marks.

**DDA corner case.** The ray loop in `draw.c` special-cases exact grid-corner crossings to avoid one-column wall gaps. Be careful changing `f_sidedx`/`f_sidedy`, `side`, or the stepping, which now lives in the `ddaWalk0`..`3` walkers (`ddaasm.a`, with its C twin in `pc/src/ddaasm_pc.c` - change both). The walk steps a pointer through `map[][]` with **no bounds check**: that is safe only because every map border is solid, which `loadMapFile` enforces and nothing at run time undoes. Anything that could open a solid cell at run time breaks that guarantee.

**Map cells are packed `u16`.** Flag bits (`MAP_MASK_SOLID`, `WALL`, `SPRITE`, `ENEMY`, `WALK`, `MARKED`) plus a 4-bit type in `MAP_BLOCK_TYPE_MASK` and a 6-bit id. `game_map.h` exposes `static` inline-style accessors (`mapCell`, `isWall`, `isSolid`, `canWalk`, …); out-of-range cells return a solid `WALL_TYPE_VOID` cell so callers never bounds-check. Map ASCII characters are decoded in `getCellEncoding()`; `C`/`E`/`F`/`G` delegate to `getEnemyCell()`.

**Timing.** 32 ticks per second (`units.h`). `runTicks()` catches up game state in whole ticks against `p_returntickcount()` before rendering once, so `updatePlayer()`/`runAI()` may run zero or many times per frame. `units.h` also carries the meters-per-second-to-map-units conversions (1 cell = 2 metres). Whenever the mode switches back to playing (mission start, resume from pause), the platform restarts `gameTime` from the tick counter, or the time spent in the menu is caught up in one frame.

**Two input paths.** Play reads key *levels* from `p_getscancodes` into the `keys` mask once a frame. Menus, and Esc during play, are key *events* from the window server (`WM_KEY`, mapped to `UI_KEY_*` in `psion3d.c`), which is why the play loop asks for `WE_KEY` and why the menus never touch `keys`. The PC host mirrors both: `hostSetKey` for levels, `hostMenuKey` for events. `menu.c` is portable and draws only through `ui.h`; the menu window is created last so it stacks over the game and debug windows, is hidden while playing (`blitVideoMem` writes video RAM underneath it), and repaints by `wInvalidateWin` so there is one draw path.

## Performance

The renderer has been through a measured optimisation pass (14 → 20fps). **Measure before optimising.** Five predictions during that pass were wrong, three of them "this is obviously faster" changes that regressed on the target.

Measured budget, device, 2026-10-01, branch `sprite_optimisation` with everything below in it (`tools\profile.bat`), ms per frame:

| Station | Frame | Rays | Walls | Sprites | Weapon | Clear | Blit | Other | Resid |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Corridor | 30.5 | 9.5 | 12.0 | 0.2 | 3.6 | 1.5 | 3.7 | -0.2 | 0.2 |
| Empty room | 24.3 | 12.2 | 3.6 | -0.1 | 3.3 | 1.2 | 3.5 | 0.3 | 0.3 |
| Detail walls | 30.2 | 10.6 | 10.9 | 0.0 | 3.4 | 1.2 | 3.5 | 0.3 | 0.3 |
| Openings | 52.9 | 14.9 | 28.6 | 0.0 | 3.3 | 1.2 | 3.5 | 0.3 | 1.1 |
| Enemies | 42.9 | 12.4 | 4.2 | 17.8 | 3.2 | 1.2 | 3.4 | 0.4 | 0.3 |
| Decorations | 45.1 | 12.5 | 3.7 | 20.3 | 3.1 | 1.3 | 3.4 | 0.3 | 0.5 |
| Crowd | 86.6 | 13.3 | 4.3 | 60.4 | 3.3 | 1.7 | 3.6 | -0.3 | 0.3 |
| **Mean** | **44.6** | **12.2** | **9.6** | **14.0** | **3.3** | **1.3** | **3.5** | **0.1** | **0.4** |

The clear was 4.1ms as an unrolled C loop; `bmasm.a`'s `rep stosw` is 1.1-1.3ms (~0.21us, ~6 clocks a word). The blit moves nearly as many words by `rep movsw` in 3.5ms (0.71us a word), so its cost is video RAM, not code.

Earlier runs of the same profile, for comparison. Before task 26: Frame 45.1 / 53.6 / 47.2 / 70.5 / 81.1 / 88.2 / 139.5 (mean 75.0), Rays mean 26.6. After the DDA work and the assembler clear, with the old sprite decoders: Frame 38.2 / 27.9 / 35.5 / 65.2 / 61.9 / 69.0 / 116.6 (mean 59.1), Rays 13.6, Walls 12.4 (Openings 38.1, Corridor 16.8 before the spans were unrolled), Sprites 32.9 / 40.4 / 86.7 on the three sprite stations, Weapon 4.8. The profile's Frame column has predicted the benchmark to within 0.5ms a station. Frame is the sum of the other columns by construction, so a misread digit shows up as a row that does not add up (this table's Decorations was first transcribed as 80.2, and chased as an 8ms build difference that never existed).

Unit costs, fitted to PC-host counts of the same frames (every station within 0.5ms; task 26 has the counts). **Rays:** 4.9ms fixed (81us a ray; ~1ms of it is the border and crosshair, and the per-ray part lost ~12.5us on 2026-10-01 when the loop-invariant set-up moved out of the loop), **3.6us per DDA step**, **24us per stop** (a wall hit or collected sprite, where the assembler walk returns to `draw()`), **52us of maths per wall hit** (~39us after 2026-10-01: two accessor calls, the index multiplies and odd-address words gone), 0.14ms per sprite projected. **Walls:** 14us per hit, **24us per span call**, 2us per row (~1.15us since the fill and clear spans were unrolled in `bmasm.a`, 2026-10-01). The step was 25.6us (~690 clocks) before task 26: TopSpeed does not inline the `static` accessors in `game_map.h`, so each step made three near calls with the loop state on the stack. Writing them out made it ~17us; the walk in assembler (`ddaasm.a`, all six values in registers, a pointer through `map[][]`) made it 3.6us; the stop was then trimmed from ~34us to ~24us (average 15.3 -> 16.9 -> 19.2 -> 19.5 fps). The weapon, clear and blit are a flat ~8.1ms on every frame (12.4 before the assembler clear, 9.3 before the weapon moved to `spriteBlitRows`). **Sprites** (`sprasm.a`, [SPRITES.md](SPRITES.md)): ~5us a drawn pixel and ~50us a row, so for the 11-30 pixel sprites on screen the row overhead is about half the cost. For sizing a change in advance, ~15 clocks per V30 instruction (~0.55us) has held against the device.

Benchmark baseline on device, 2026-10-01, branch `sprite_optimisation` at its wrap-up: everything on main (DDA work, assembler clear, wall hit maths, per-ray set-up, `cpu=>286`, unrolled spans; average 22.9) plus the direct sprite draw and weapon blit in `sprasm.a` with lean rows and inlined middle bytes (Options → Benchmark, `map97.map`; a change's numbers go next to these in its commit message). Two runs agreed to 0.2 fps on one station and exactly elsewhere, so treat a change under 0.3 fps as noise and 0.5 as real - on the 20-40 fps stations; on the crowd at 11.5 fps 0.3 fps is already 2.3ms, and a lone reading there should be repeated before it is acted on:

| Station | fps | ms/frame |
| --- | --- | --- |
| Corridor | 33.4 | 30 |
| Empty room | 41.2 | 24 |
| Detail walls | 33.4 | 30 |
| Openings | 19.1 | 52 |
| Enemies | 23.4 | 43 |
| Decorations | 22.2 | 45 |
| Crowd | 11.5 | 87 |
| **Average** | **26.3** | |

Read it as: walls are cheaper than sprites, and sprite *rows* are what cost — one decoration at 2 cells (Decorations) is worse than six enemies further off, and the crowd's heavy filling the screen is an 87ms frame. Openings at 52ms, now the slowest station but the crowd, is the arch reveals and the second face behind every see-through cell. The empty room, the DDA-heavy case at 12–16 cells a ray, is now the fastest station: it was 53ms before task 26 made the step 7x cheaper.

**Span call count dominates wall cost, not rows written.** Three dither bands covering 0.31× the column height measured 5.6ms — 2.6× the per-row cost of the full-height span they sit on. Fewer, larger span calls win; splitting a style into *more* calls to write *fewer* rows loses. `WALL_DETAIL_DEPTH` and the `LAB_PANEL_*` switches in `walls.h` are the tunables, with their measured costs documented there.

**How to measure.** Add a `#define` that *removes* work, rebuild, and run **Options → Benchmark** on the device: it steps through the seven stations of `map/map97.map` (corridor, empty room, detail walls, openings, enemies, decorations, crowd), five seconds each with input ignored and AI frozen, and ends on a results screen with a frame rate per station in tenths and the run's average (`bench.c`, task 19). Copy that line into the commit message. One fps step is ~2.5ms at 20fps. A station is a `station = x, y, bearing, name` line in the map file, so a new aspect is a new room and a line, not a code change; `psion3d_pc --map 97 --station N` shows what a station sees, and the `bench_*` golden frames pin every scene. The HUD's FPS row (Options) is still there for an ad-hoc reading, but measure from a fixed position — active enemies move and make readings unstable.

**Where the time goes, per station:** `.\tools\profile.bat` builds `PROF3D.IMG`, whose benchmark runs each station in eight ablation passes (~6 minutes) and ends on a table of ms per frame for rays, walls, sprites, weapon, clear, blit and the loop around them, plus a residue that should be noise (task 26, `bench.h`). Use it before choosing what to optimise; use the normal benchmark to measure the change. Its hooks are `#ifdef BENCH_PROFILE` and must stay so — `PSION3D.IMG` has to hash the same with or without them.

Never isolate ray-loop internals by *substituting* values: everything downstream depends on the ray's result, and three such attempts came back contaminated, two reading **slower** than baseline. For work that cannot simply be removed, do it **twice** and discard the copy — the delta is one pass.

**Already tried and rejected. Do not re-propose without new evidence:**

| Idea | Why it failed |
| --- | --- |
| Interleaved backbuffer (nibble or byte) | The LCD is planar, so the blit must gather: +28ms/frame measured. `rep movsw` is unbeatable, which makes the planar layout load-bearing. |
| Fusing both planes into one span loop (`bmSpan4`) | Register spill — six mask bytes plus two pointers on a four-register CPU. Reached parity at best. |
| Coarse block skip in the DDA | map1 is 98% solid wall with ~75 walkable cells and **zero** empty 8×8 blocks. Check the map before any spatial optimisation. |
| 32×32 textured walls | 16fps against 20 at best, after three implementations. Code removed. |
| Run-length texture rendering | Fixed 32 texel iterations per column, but `wallHeight` is `30720 / distance` so a wall 4 cells away is only 30 rows tall — fewer rows than iterations. |
| Colour-run (RLE / transposed Doom post) sprite format | Measured 2026-09-21 with the decoder in C and then in assembler, identical numbers: Corridor 22.2 → 17.7, Enemies 12.4 → 9.6, Crowd 6.7 → 6.3. At on-screen sizes (11–30 px) a run is 1–4 destination pixels, so 1.6–2.8× fewer units at 2–3.5× the cost each; and the weapon lost its 4-pixels-per-lookup 1:1 path. TopSpeed's code for that decoder was as tight as hand assembler - true of that loop, not in general (next row). Task 22 has the figures. |
| A scaled sprite draw in C with no decode | Measured 2026-10-01: TopSpeed spills the whole pixel loop to the stack (~34 instructions, ~20us a pixel), Sprites Enemies 32.9 -> 39.1ms, Crowd 86.7 -> 164.9. The same loop in `sprasm.a` with its state in registers is what shipped (17.8 / 60.4). `tsda` the C before deciding it cannot be beaten. |

## Assets

Files are opened from full Psion paths: maps from `LOC::M:\IMG\MAP\map<N>.map`, sprites from `LOC::M:\IMG\SPR\<base>.spr`. `p_read()` signals EOF with `E_FILE_EOF`, not `0` — treat any other short read as an error. Map loading writes straight into `map[][]`; do not add a second load buffer. The file is sectioned text - `[MAP]` grid, `[LEVEL]` key=value (start cell, compass bearing, end cell, title, location, map position), `[BRIEFING]` and up to five `[OBJECTIVE]` blocks - specified in `map/README.md`. Numbers land in `mapInfo`; the prose streams into a far segment and is read back with `mapTextCopy()`, so level text costs no DGROUP. `psion3d_pc --map N -v` prints what was parsed, and a malformed file fails the load with the line number.

Sprites are 64x64, one file per sprite, `<base>.spr`, holding its 1-8 frames back to back at 1,104 bytes each - the 1,024-byte row-major 2bpp payload, 64 row-span bytes, then a paragraph of box, `SPR` tag and format 2, frame count and frame index. A frame is stored exactly as it sits in the segment and the cache, so `loadSprite("sci", slot)` opens the file once, sizes the segment from the first frame's count and copies; the converter computes everything (SPRITES.md has the layout). Slot ids are in `sprslot.h`. Pixel values: `0` transparent, `1` grey, `2` black, `3` white.

Convert PNGs with `tools\convert_sprite.bat`. For raw output use `.\tools\convert_sprite.bat /f sprites\sci spr\sci.spr` (a base path takes `sci0.png`, `sci1.png` ... until one is missing; a single `.png` makes a one-frame sprite) or `-OutputPath`, and `tools\convert_all_sprites.bat` to redo every sprite in `sprites\` — do not use PowerShell redirection, it corrupts binary output. Pass `-WhiteTransparent true` only when near-white should become transparent (the default preserves it as white).

Sounds are `.wve` files: a 32-byte PLIB `SndFile` header, then 8kHz A-law, one byte a sample (8,000 bytes a second). Convert WAVs with `.\tools\convert_sound.bat in.wav out.wve`; it resamples and encodes, and `-Gain 100` (default) matches the SDK's `wav2wve`, which uses only half the codec's range - `-Gain 200` is full scale, `-Normalize` puts the peak at 4095. The Psion's A-law sign bit is set for *negative* samples, the reverse of most G.711 code, so do not swap in a library encoder. AGENTS.md has the header layout and how the converter was checked. Playback is one channel with no mixer (task 12).

## Working style

Keep edits narrow. Do not reformat the generated trig table or other large tables unless asked, and do not delete the generated Psion artifacts (`*.OBJ`, `PSION3D.EXE`, `PSION3D.IMG`, `PSION3D.MAP`) unless explicitly asked to clean up. Follow the existing C style: tabs, braces on their own line, `static` helpers, project typedefs (`s16`, `u16`, `f16`, `s32`), and an `f_` prefix on fixed-point variables where scale matters.
