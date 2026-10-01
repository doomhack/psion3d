# Memory budget

Measured 2026-10-01 from `PSION3D.MAP`, clean tree at `59faa59` (task 26,
profiling and the renderer optimisations). Re-measure with the recipe at the
end whenever a global array is added; the numbers here go stale, the shape of
the budget does not. The history table is appended by `memcheck -Record` on
every change and is the current figure when it disagrees with the tables above
it - refresh those when it does.

## The three limits

The build is `#model small jpi`: near code and near data are separate 64 KB
segments, and the SIBO machine has 512 KB total.

| Limit | Used | Of | Headroom |
| --- | ---: | ---: | ---: |
| Near code (`_TEXT`) | 49,052 | 65,536 (74.8%) | 16,484 |
| **Near data (DGROUP)** | **47,616** | **65,536 (72.7%)** | **17,920** |
| System RAM (process + RAM disk) | ~276 KB | 512 KB (54%) | ~236 KB less OS |

**DGROUP is the binding constraint.** Every global, `static` and string
literal lands there, and `memcheck` fails the build check above 48 KB - 1,536
bytes away. Task 26's optimisations added nothing to it but an 8 byte table.

**Code has overtaken it in share of its segment** (74.8% against 72.7%), and is
the one growing: +4,080 since 2026-09-22, nearly all of it the assembler of
task 26. Of `bmasm.a`'s 3,509 bytes, 3,200 are the unrolled row tables of the
three wall spans (640 + 640 + 1,920), bought for ~11 ms a frame on the
wall-heavy benchmark stations; unrolling anything else is a code decision as
much as a speed one. The 512 KB figure is not a useful ceiling for anything
but the sprite asset count.

The linker map lays code, stack and DGROUP out linearly and the total is over
64 KB (`0x189A8`); that is fine because `CS` and `DS` are separate selectors.
Do not read the image total as a segment limit.

## Program image - 100,776 bytes resident

| Segment | Class | Bytes | What it is |
| --- | --- | ---: | --- |
| `_TEXT` | CODE | 49,052 | all C and assembler modules plus PLIB/WLIB stubs, by module below |
| `STACK` | STACK | 4,096 | see stack below |
| `_MAGIC` | MAGIC | 4,096 | SIBO process control area, fixed by the runtime |
| `_INIT` | DATA | 2 | |
| `_CONST` | DATA | 4,249 | `sincos_tab` 2,048, `enemyStats`, `ammoTypes`, `weapons`, `ddaWalkers` 8, and ~1,800 of string literals and const tables (menu labels, cheat names, HUD text) |
| `_DATA` | DATA | 387 | initialised globals: `rayIdxOffset` 120, `player`, `blackBm`/`greyBm`, bitmap masks, RNG seeds, mode/mission/settings/bench/cheat state |
| `_BSS` | BSS | 38,875 | zero-initialised globals - the real cost, broken down below |
| `BSS_END` | BSS | 7 | |

DGROUP is `_MAGIC` through `BSS_END`: 4,096 + 2 + 4,249 + 387 + 38,875 + 7
plus alignment = 47,616.

The `.IMG` file (53,760 bytes) is `_TEXT` + `_CONST` + `_DATA` + headers.
`STACK`, `_MAGIC` and `_BSS` are allocated at load and cost nothing on disk.

The profiling build (`PROF3D.IMG`, `tools\profile.bat`) is not this image: its
DGROUP is 47,952, +336 for the pass tables, 1,200 to the limit. Keep it inside
the limit too.

## `_TEXT` by module - 49,052 bytes

Only extern functions are published, so a module is measured from its lowest
public to the next module's; a module's leading `static` functions are counted
in the one before it. Treat the figures as +/- a couple of hundred bytes.
`psion3d.c` publishes only `main`, so its statics sit after `bmasm.a`; that
split is taken from the end of `bmasm.a`'s pattern table (0xB975).

| Module | Bytes | | Module | Bytes |
| --- | ---: | --- | --- | ---: |
| `menu.c` | 6,633 | | `hud.c` | 1,925 |
| `walls.c` | 5,985 | | `player.c` | 1,865 |
| `draw.c` | 5,923 | | `mission.c` | 1,287 |
| `enemy.c` | 4,584 | | `ui_psion.c` | 1,203 |
| `bmasm.a` | 3,509 | | `labwall.c` | 1,133 |
| `sprite.c` | 3,270 | | `psion3d.c` + `fpasm.a` | 812 |
| `game_map.c` | 3,233 | | `cheat.c` | 583 |
| `bitmap.c` | 2,229 | | `automap.c` + `bench.c` | 477 |
| `gameloop.c` | 2,215 | | `pickup.c` + `decor.c` + `level.c` | 424 |
| SDK start-up and stubs, `N$` helpers | 1,346 | | `ddaasm.a` + `videomem.a` | 416 |

Grouped: renderer (bitmap, draw, walls, labwall, sprite, and the assembler
`bmasm`, `ddaasm`, `videomem`) ~22.5 KB, up from 18.2, all of it task 26;
game (enemy, player, map, gameloop, pickups) ~12.3 KB; front end (menu,
mission, hud, automap, bench, cheat, ui) ~12.1 KB; runtime and `main` ~2.2 KB.
`debug.c`, `fp_math.c` and `settings.c` contribute data only. `bitmap.c` lost
~420 bytes to `bmasm.a` (the clear and the three spans moved out).

## `_BSS` - 38,875 bytes

The linker publishes only extern symbols, so file-scope `static`s appear as
gaps between consecutive publics. Regions below are those gaps, attributed from
the declarations.

| Region | Bytes | Owner | Notes |
| --- | ---: | --- | --- |
| `screenBm` | 10,240 | `bitmap.c` | both bitplanes, 320 rows x 32 B. Load-bearing, see CLAUDE.md |
| `spriteCache` | 9,216 | `sprite.c` | 9-frame near-RAM LRU; keeps far sprite count decoupled from DGROUP. Nine because the Decorations benchmark station cycles nine frames, and LRU one short of the working set misses every access (3.2 ms) |
| `map[64][64]` | 8,192 | `game_map.c` | packed `u16` cells |
| `spriteFrameBounds` | 3,072 | `sprite.c` | 32 slots x 8 frames x 12 B - **11 slots used, ~2 KB reclaimable** |
| `recipTab` | 2,048 | `draw.c` | `rayDelta()` per trig entry, hoists a divide out of the ray loop |
| `enemyList` | 1,792 | `enemy.c` | 64 x 28 B `enemy_t` |
| `menuText` and line table | 1,504 | `menu.c` | 1,280 byte copy of the briefing or objective text being shown, 48 line starts/lengths, two 40 byte strings |
| `spriteLoadBuffer` | 1,024 | `sprite.c` | file staging, used only during `loadSprite` |
| sprite masks | 768 | `sprite.c` | three 256-entry LUTs |
| `spriteCol` / `ColShift` / `RowPix` | 545 | `sprite.c` | per-column source byte or column, bit shift (small sprites), and the current source row unpacked a pixel per byte (magnified sprites) |
| `mapInfo` | ~88 | `game_map.c` | level numbers and text offsets, 8 benchmark stations |
| `benchFps10` | 16 | `bench.c` | per-station result |
| everything else | ~435 | | sprite row masks and `spriteColVisible` (30, the per-column wall clip), segment handles and frame counts, cache entries, `objectiveState`, window and font statics, runtime |

Sprite working set total (`drawWall` through `map` in the map): 14,895, up 95
from 2026-09-22 with the sprite clipping and decoder work. The profiling
build adds `passFrames` / `passTicks` (2 x 128) and `benchSkip` to `bench.c`.

## Far segments - 55,760 bytes

`loadSprite()` allocates one `p_sgcreate(E_SEGMENT_HIGH)` segment per slot,
sized to the frames actually found (1,024 B each, 64 paragraphs). These cost
**nothing in DGROUP**, which is why adding art is cheap and adding globals is not.

| Segment | Base | Frames | Bytes |
| --- | --- | ---: | ---: |
| `CIV` / `MER` / `SGR` / `HVY` | `sci` `mer` `sgr` `hvy` | 8 each | 32,768 |
| `PISTOL` / `SMG` / `AR` / `LMG` | `ppk2` `mp5` `ak` `m249` | 2 each | 8,192 |
| `PICKUPS` | `pup` | 4 | 4,096 |
| `DECORATIONS` | `dec` | 4 | 4,096 |
| `PARTICLES` | `hit` | 2 | 2,048 |
| **11 of 32 sprite slots** | | **50** | **51,200** |
| `MAPTXT` | `game_map.c` level prose | | 3,072 |
| `MISIDX` | `mission.c` 20 missions x 74 B, with best times | | 1,488 |

The history table's Far column, and `memcheck`, count sprites only.

Full sprite capacity is 32 slots x 8 frames = 256 KB, which would on its own be
half the machine. Nothing enforces a ceiling; keep an eye on it as slots fill.

`DIRECT_VIDEO_MEM_ACCESS` is defined in `psion3d.c`. The compatibility blit
path it disables would add one 10,240-byte segment-backed WLIB bitmap.

## Stack - 4,096 allocated, ~500 used

Measured 2026-10-01 from the `sub sp` in each function's `tsda` listing.
Deepest is `draw()`: 374 bytes of locals (0x176) - `f_wallDepth[60]` 120,
`spriteHits[8]`, `markedSprites[8]` 16, `wallhits[3]` 42 since `wallhit_t`
went to 14 bytes, the `ddaray_t` block 10, the hoisted per-frame values and
scalars - plus five saved registers, ~390 bytes, up from ~290. Under it
`drawProjectedSprite` takes 92 + 10 and the wall styles and `ddaWalk` walkers
a few words each, so ~500 at the deepest. `loadMapFile()` at 226 (its 64 byte
read chunk and parser state) and `loadSprite()` at 94 are the next largest,
and never under `draw()`.

The unused ~3.6 KB is the cheapest reserve if DGROUP ever gets tight: the stack
is not part of DGROUP, but shrinking it and moving a table there is a single
edit in the project file.

## RAM disk - ~125,615 bytes

Assets are opened from `LOC::M:\IMG\...`, so if the game is installed on the
internal RAM disk its files are also RAM.

| File | Bytes |
| --- | ---: |
| `PSION3D.IMG` | 53,760 |
| 50 x `.spr` at 1,040 | 52,000 |
| 4 x `.map` (`map1`, `map97`-`map99`) | 19,855 |

Installing to an SSD (`A:` / `B:`) removes this from the budget entirely.

## Totals

| | Bytes | % of 512 KB |
| --- | ---: | ---: |
| Program image | 100,776 | 19.2% |
| Far segments | 55,760 | 10.6% |
| **Process** | **156,536** | **29.9%** |
| RAM disk | 125,615 | 24.0% |
| **Total** | **~276 KB** | **53.8%** |

The OS, window server and file system take their own share on top.

## History

| Date | `_TEXT` | DGROUP | Far | Note |
| --- | ---: | ---: | ---: | --- |
| 2026-09-05 | 15,468 | 44,272 | 35,840 | baseline |
| 2026-09-05 | 15,038 | 42,214 | 35,840 | test-pattern fallback removed: −2,058 DGROUP |
| 2026-09-11 | 22,760 | 42,400 | 49,152 | pickups, decorations, tracers, `gameloop.c` |
| 2026-09-12 | 24,634 | 42,816 | 49,152 | AI memory fields, player collision, hit feedback |
| 2026-09-16 | 25,385 | 42,864 | 49,152 | detail walls, strafe keys, memcheck script |
| 2026-09-19 | 28,885 | 42,992 | 49,152 | map format: mapInfo + text segment handle |
| 2026-09-19 | 37,731 | 45,120 | 49,152 | menu system: menu.c buffers, mission index, automap, ui seam |
| 2026-09-20 | 39,665 | 45,280 | 50,176 | mission outcome: timer, best times, KIA, outcome screen |
| 2026-09-20 | 40,627 | 45,376 | 50,176 | options screen: settings.c |
| 2026-09-20 | 41,756 | 45,392 | 50,176 | HUD: hud.c, HUD window replaces the debug window |
| 2026-09-21 | 42,430 | 45,392 | 50,176 | HUD cells: per-value updates through gPrintBoxText |
| 2026-09-21 | 43,636 | 45,568 | 51,200 | task 19: benchmark (bench.c, stations in mapInfo, results screen) |
| 2026-09-21 | 43,636 | 46,592 | 51,200 | task 22: sprite frame cache 8 -> 9 slots (Decorations thrash, 3.2 ms) |
| 2026-09-22 | 44,972 | 47,504 | 51,200 | task 24: cheats (cheat.c, Cheats screen, 16 hooks) |
| 2026-09-30 | 45,798 | 47,616 | 51,200 | sprite decoder and clipping, mission index (97ad1b3..08d632b); task 26 profiling build, nothing in this image |
| 2026-09-30 | 46,206 | 47,616 | 51,200 | task 26: DDA walk in assembler (ddaasm.a), solid map border check, walker table 8 B |
| 2026-09-30 | 46,127 | 47,616 | 51,200 | task 26: screen clear as `rep stosw` (bmasm.a) |
| 2026-10-01 | 46,097 | 47,616 | 51,200 | task 26: wall hit maths, per-ray set-up hoisted |
| 2026-10-01 | 45,869 | 47,616 | 51,200 | task 26: `cpu=>286`, push-immediate only |
| 2026-10-01 | 47,155 | 47,616 | 51,200 | task 26: fill and clear spans unrolled (2 x 640 B of row table) |
| 2026-10-01 | 49,052 | 47,616 | 51,200 | task 26: dither span unrolled (1,920 B of row table) |

Code has grown ~24 KB since 2026-09-12 and data ~4.8 KB, of which 1 KB was
the ninth sprite cache frame and most of the rest `menu.c`'s buffers and
`_CONST` strings. Task 26 added 4.1 KB of code and 8 bytes of data for a
15.3 -> 22.9 fps benchmark: the trade the budget wants, speed bought in the
segment with room. Features should keep landing as code and far data, not as
near arrays - but `_TEXT` is now the fuller segment by share, with 16 KB left.

## Rules of thumb

- **Before adding a global array, check DGROUP.** Run `.\tools\memcheck.bat`,
  or read `__bss_end` in the map relative to the DGROUP segment paragraph
  (e.g. `0CFA:BA00` → 47,616).
- **Bulk data goes far.** Sprites, level prose and the mission index already
  do. A 128x128 map is 32,768 bytes against 17,920 free and cannot live in
  `map[][]` as declared; it would need a far segment and accessor changes in
  `game_map.h`.
- **`{0}` initialisers move a variable from `_BSS` to `_DATA`.** Same DGROUP
  cost, but it is then stored in the `.IMG` too. Leave large zeroed arrays
  uninitialised.
- **String literals are near data.** Every menu label and `drawDbgText("...")`
  format costs its length in `_CONST`.
- **Do not re-add a near-data fallback sprite.** The test pattern cost 2 KB of
  DGROUP and both `getSpriteFrame` callers already handle a missing slot.

## How to re-measure

Build, then run `.\tools\memcheck.bat`. It prints `_TEXT`, DGROUP and far
sprite bytes with the delta against the last history row, plus the DGROUP
segment breakdown, and exits 1 above 48 KB (`-Limit`). `-Record "note"`
appends a row to the history table above. To attribute a change, keep a copy of
the previous `PSION3D.MAP` and pass `-Baseline old.MAP`: the report then
lists every DGROUP region whose size moved.

By hand, read the segment table at the top of `PSION3D.MAP`:

```bash
"C:\Program Files (x86)\DOSBox-0.74-3\DOSBox.exe" -c "D:" -c "tsc /m unnamed.pr /smain=psion3d /v0 /zq > D:\build.log" -c "exit"
```

```bash
sed -n '3,12p' PSION3D.MAP
```

DGROUP used is the `__bss_end` offset. To attribute a gap between two publics,
list the `static`s in the modules linked between them - link order follows
`unnamed.pr`. `_TEXT` per module works the same way on the `0000:` publics.
Far sprite total is frames loaded in `loadMapData()` x 1,024.
