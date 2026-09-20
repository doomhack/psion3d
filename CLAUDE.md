# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

`AGENTS.md` in the repo root holds the longer-form conventions (style, sprite tooling details, commit/PR guidance). Read it before non-trivial changes; this file is the fast orientation.

`TASKS.md` is the project task list - planned features and their current state. Check it before starting new work, and tick items off there as they land.

`MEMORY_BUDGET.md` is the measured memory breakdown - near data (DGROUP) is the binding limit at ~65% of 64 KB, not the 512 KB system total. Check it before adding a global array, and run `.\tools\memcheck.bat` after the build to see the numbers and the delta (`-Baseline old.MAP` names the regions that moved; `-Record "note"` appends the history row).

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
| `hud.c` | The in-game HUD panels (health, objectives, weapons/ammo, fps) drawn through `ui.h` into the HUD target: static parts on a redraw event, values as single-call cells replaced only when they change (never invalidate the HUD window for a value; see task 8) |
| `draw.c` | DDA ray cast, per-span wall depth buffer, sprite collection/sorting, player shot resolution |
| `walls.c` | Default wall style + `drawWall` function-pointer global |
| `labwall.c` | Lab environment wall style (`drawWallLab`) |
| `bitmap.c` | Local 1bpp screen buffers and fill/clear/pattern/line primitives |
| `videomem.a` | JPI assembler `blitVideoMem()` — direct writes to segment 0x40 video RAM |
| `fpasm.a` | JPI assembler `fpmul()` — the Q8 multiply, using the V30's native `IMUL` |
| `sprite.c` | `.spr` loading into segments, frame cache, projection, drawing. A slot that failed to load draws nothing; there is no built-in fallback sprite |
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

**Assembler modules.** `.a` files are JPI/TopSpeed assembler, listed in `unnamed.pr` alongside the C modules. The syntax has traps: `;` comments are a **syntax error**, and displacements concatenate brackets — `mov es:[di][9600],al`, not `[di+9600]`. Arguments arrive in `AX`, `BX`; declare the convention explicitly with `#pragma call(reg_param=>(...), reg_saved=>(...))` on the C prototype rather than relying on the default.

**Two bitplanes in one buffer.** `screenBm` is a single 256x320 1bpp buffer: the top 256x160 half is the black plane, the bottom half is grey. `blackBm` and `greyBm` are pointers *into* `screenBm`, not separate allocations. Rows are 32 bytes wide, so byte addressing is `y << 5` and word addressing `y << 4`. Pixels are **low-bit-first**: pixel `x` uses `1 << (x & 7)`. Reversing that bit order swaps column pairs and produces jagged wall edges.

**The black plane sits on top of the grey plane.** A pixel set in both reads as black, so the display shows three shades — background, grey, black — not four. Most black on screen arrives this way rather than from the black plane alone, so treating "both set" as a separate darker shade visibly mis-renders the majority of it.

**Two blit paths.** `psion3d.c` defines `DIRECT_VIDEO_MEM_ACCESS`, which routes through the assembler `blitVideoMem()` (disables memory protection via port 0x15, `rep movsw` into segment 0x40, re-enables via port 0x14). The `#else` branch is the compatibility path: one `p_sgcopyto()` of the whole buffer to a segment-backed WLIB bitmap, then two `gCopyBit` calls. Keep both working. Outside of `videomem.a`, do not manipulate `DS`/`ES`, `cli`/`sti`, protection ports, or OS handle-table segments from C — past experiments with that crashed the emulator.

**Wall rendering boundary.** `draw.c` computes `wallHeight`, `f_wallDist`, `f_wallX`, `cell`, and `side` into a `wallhit_t`, then calls through the `drawWall` global. Environment-specific appearance, `WALL_TYPE_*` dispatch, doors, windows, and shading belong in a wall-style module (`walls.c`, `labwall.c`), selected per level in the `loadMapData()` `mapId` switch. A wall draw function returns `TRUE` when the column should write the wall depth buffer and `FALSE` for openings and non-occluding features — sprite occlusion and weapon impact resolution both depend on that contract.

**Column geometry.** Wall columns are 4 pixels wide, `x` aligned to a nibble boundary. Use `bmFillRect4` / `bmClearRect4` / `bmFillPattern4` for those; the general `bmFillRect` / `bmClearRect` / `bmFillPattern` are for variable-width sprites, UI, and 1-pixel detail marks.

**DDA corner case.** The ray loop in `draw.c` special-cases exact grid-corner crossings to avoid one-column wall gaps. Be careful changing `f_sidedx`/`f_sidedy`, `side`, or `mapx`/`mapy` stepping.

**Map cells are packed `u16`.** Flag bits (`MAP_MASK_SOLID`, `WALL`, `SPRITE`, `ENEMY`, `WALK`, `MARKED`) plus a 4-bit type in `MAP_BLOCK_TYPE_MASK` and a 6-bit id. `game_map.h` exposes `static` inline-style accessors (`mapCell`, `isWall`, `isSolid`, `canWalk`, …); out-of-range cells return a solid `WALL_TYPE_VOID` cell so callers never bounds-check. Map ASCII characters are decoded in `getCellEncoding()`; `C`/`E`/`F`/`G` delegate to `getEnemyCell()`.

**Timing.** 32 ticks per second (`units.h`). `runTicks()` catches up game state in whole ticks against `p_returntickcount()` before rendering once, so `updatePlayer()`/`runAI()` may run zero or many times per frame. `units.h` also carries the meters-per-second-to-map-units conversions (1 cell = 2 metres). Whenever the mode switches back to playing (mission start, resume from pause), the platform restarts `gameTime` from the tick counter, or the time spent in the menu is caught up in one frame.

**Two input paths.** Play reads key *levels* from `p_getscancodes` into the `keys` mask once a frame. Menus, and Esc during play, are key *events* from the window server (`WM_KEY`, mapped to `UI_KEY_*` in `psion3d.c`), which is why the play loop asks for `WE_KEY` and why the menus never touch `keys`. The PC host mirrors both: `hostSetKey` for levels, `hostMenuKey` for events. `menu.c` is portable and draws only through `ui.h`; the menu window is created last so it stacks over the game and debug windows, is hidden while playing (`blitVideoMem` writes video RAM underneath it), and repaints by `wInvalidateWin` so there is one draw path.

## Performance

The renderer has been through a measured optimisation pass (14 → 20fps). **Measure before optimising.** Five predictions during that pass were wrong, three of them "this is obviously faster" changes that regressed on the target.

Measured budget, map 1, fixed corridor position, 20fps = 50ms/frame:

| Component | Cost |
| --- | --- |
| Wall drawing (~10ms flat column fill, ~12.5ms style detail) | 22.5ms |
| Ray cast (per-ray setup, DDA walk, per-hit maths) | ~11ms |
| Weapon overlay sprite | ~4ms |
| Screen clear, blit, enemy sprites, AI, `wFlush` combined | ~12ms |

**Span call count dominates wall cost, not rows written.** Three dither bands covering 0.31× the column height measured 5.6ms — 2.6× the per-row cost of the full-height span they sit on. Fewer, larger span calls win; splitting a style into *more* calls to write *fewer* rows loses. `WALL_DETAIL_DEPTH` and the `LAB_PANEL_*` switches in `walls.h` are the tunables, with their measured costs documented there.

**How to measure.** Add a `#define` that *removes* work, rebuild, read the fps counter (`psion3d.c` prints it once a second). Watch it ~10 seconds; one fps step is ~2.5ms at 20fps. Measure from a fixed position — active enemies move and make readings unstable.

Never isolate ray-loop internals by *substituting* values: everything downstream depends on the ray's result, and three such attempts came back contaminated, two reading **slower** than baseline. For work that cannot simply be removed, do it **twice** and discard the copy — the delta is one pass.

**Already tried and rejected. Do not re-propose without new evidence:**

| Idea | Why it failed |
| --- | --- |
| Interleaved backbuffer (nibble or byte) | The LCD is planar, so the blit must gather: +28ms/frame measured. `rep movsw` is unbeatable, which makes the planar layout load-bearing. |
| Fusing both planes into one span loop (`bmSpan4`) | Register spill — six mask bytes plus two pointers on a four-register CPU. Reached parity at best. |
| Coarse block skip in the DDA | map1 is 98% solid wall with ~75 walkable cells and **zero** empty 8×8 blocks. Check the map before any spatial optimisation. |
| 32×32 textured walls | 16fps against 20 at best, after three implementations. Code removed. |
| Run-length texture rendering | Fixed 32 texel iterations per column, but `wallHeight` is `30720 / distance` so a wall 4 cells away is only 30 rows tall — fewer rows than iterations. |

## Assets

Files are opened from full Psion paths: maps from `LOC::M:\IMG\MAP\map<N>.map`, sprites from `LOC::M:\IMG\SPR\<base><frame>.spr`. `p_read()` signals EOF with `E_FILE_EOF`, not `0` — treat any other short read as an error. Map loading writes straight into `map[][]`; do not add a second load buffer. The file is sectioned text - `[MAP]` grid, `[LEVEL]` key=value (start cell, compass bearing, end cell, title, location, map position), `[BRIEFING]` and up to five `[OBJECTIVE]` blocks - specified in `map/README.md`. Numbers land in `mapInfo`; the prose streams into a far segment and is read back with `mapTextCopy()`, so level text costs no DGROUP. `psion3d_pc --map N -v` prints what was parsed, and a malformed file fails the load with the line number.

Sprites are 64x64, one frame per file, `base0.spr` … `base7.spr`; `loadSprite("sci", slot)` loads frame 0 then consecutive frames until one is missing, and sizes the segment to the frames actually loaded. Slot ids are in `sprslot.h`. A frame file is 1040 bytes — 16-byte header + 1024-byte row-major 2bpp payload; only the paragraph-aligned payload is copied into the segment. Pixel values: `0` transparent, `1` grey, `2` black, `3` white.

Convert PNGs with `tools\convert_sprite.bat`. For raw output use `.\tools\convert_sprite.bat /f tools\health.png health0.spr` or `-OutputPath` — do not use PowerShell redirection, it corrupts binary output. Pass `-WhiteTransparent true` only when near-white should become transparent (the default preserves it as white).

## Working style

Keep edits narrow. Do not reformat the generated trig table or other large tables unless asked, and do not delete the generated Psion artifacts (`*.OBJ`, `PSION3D.EXE`, `PSION3D.IMG`, `PSION3D.MAP`) unless explicitly asked to clean up. Follow the existing C style: tabs, braces on their own line, `static` helpers, project typedefs (`s16`, `u16`, `f16`, `s32`), and an `f_` prefix on fixed-point variables where scale matters.
