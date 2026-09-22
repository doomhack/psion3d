# Memory budget

Measured 2026-09-22 from `PSION3D.MAP`, clean tree at `205e35e` (task 24,
cheats). Re-measure with the recipe at the end whenever a global array is
added; the numbers here go stale, the shape of the budget does not. The history
table is appended by `memcheck -Record` on every change and is the current
figure when it disagrees with the tables above it - refresh those when it does.

## The three limits

The build is `#model small jpi`: near code and near data are separate 64 KB
segments, and the SIBO machine has 512 KB total.

| Limit | Used | Of | Headroom |
| --- | ---: | ---: | ---: |
| Near code (`_TEXT`) | 44,972 | 65,536 (68.6%) | 20,564 |
| **Near data (DGROUP)** | **47,504** | **65,536 (72.5%)** | **18,032** |
| System RAM (process + RAM disk) | ~267 KB | 512 KB (52%) | ~245 KB less OS |

**DGROUP is the binding constraint.** Every global, `static` and string
literal lands there, and `memcheck` fails the build check above 48 KB - 1,648
bytes away. Code is no longer far behind: it has almost doubled since
2026-09-12, nearly all of it the front end (menus, HUD, mission index,
benchmark, cheats). The 512 KB figure is not a useful ceiling for anything but
the sprite asset count.

The linker map lays code, stack and DGROUP out linearly and the total is over
64 KB (`0x17946`); that is fine because `CS` and `DS` are separate selectors.
Do not read the image total as a segment limit.

## Program image - 96,582 bytes resident

| Segment | Class | Bytes | What it is |
| --- | --- | ---: | --- |
| `_TEXT` | CODE | 44,972 | all C and assembler modules plus PLIB/WLIB stubs, by module below |
| `STACK` | STACK | 4,096 | see stack below |
| `_MAGIC` | MAGIC | 4,096 | SIBO process control area, fixed by the runtime |
| `_INIT` | DATA | 2 | |
| `_CONST` | DATA | 4,237 | `sincos_tab` 2,048, `enemyStats`, `ammoTypes`, `weapons`, and ~1,800 of string literals and const tables (menu labels, cheat names, HUD text) |
| `_DATA` | DATA | 383 | initialised globals: `rayIdxOffset` 120, `player`, `blackBm`/`greyBm`, bitmap masks, RNG seeds, mode/mission/settings/bench/cheat state |
| `_BSS` | BSS | 38,780 | zero-initialised globals - the real cost, broken down below |
| `BSS_END` | BSS | 6 | |

DGROUP is `_MAGIC` through `BSS_END`: 4,096 + 2 + 4,237 + 383 + 38,780 + 6
plus alignment = 47,504.

The `.IMG` file (49,664 bytes) is `_TEXT` + `_CONST` + `_DATA` + headers.
`STACK`, `_MAGIC` and `_BSS` are allocated at load and cost nothing on disk.

## `_TEXT` by module - 44,972 bytes

Only extern functions are published, so a module is measured from its lowest
public to the next module's; a module's leading `static` functions are counted
in the one before it. Treat the figures as +/- a couple of hundred bytes.

| Module | Bytes | | Module | Bytes |
| --- | ---: | --- | --- | ---: |
| `menu.c` | 6,679 | | `hud.c` | 1,950 |
| `walls.c` | 5,994 | | `player.c` | 1,877 |
| `draw.c` | 5,961 | | `mission.c` | 1,355 |
| `enemy.c` | 4,590 | | `ui_psion.c` | 1,237 |
| `game_map.c` | 3,170 | | `labwall.c` | 1,139 |
| `bitmap.c` | 2,653 | | `cheat.c` | 583 |
| `sprite.c` | 2,387 | | `automap.c` + `bench.c` | 479 |
| `gameloop.c` | 2,235 | | `pickup.c` + `decor.c` + `level.c` | 428 |
| `psion3d.c` + `fpasm.a` | ~815 | | `videomem.a` | 92 |
| runtime start-up | 575 | | SDK stubs, `N$` helpers | 773 |

Grouped: renderer (bitmap, draw, walls, labwall, sprite, videomem) ~18.2 KB;
game (enemy, player, map, gameloop, pickups) ~12.3 KB; front end (menu,
mission, hud, automap, bench, cheat, ui) ~12.3 KB; runtime and `main` ~2.2 KB.
`debug.c`, `fp_math.c` and `settings.c` contribute data only.

## `_BSS` - 38,780 bytes

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
| `spriteColByte` / `ColShift` | 480 | `sprite.c` | per-column blit LUTs |
| `mapInfo` | ~88 | `game_map.c` | level numbers and text offsets, 8 benchmark stations |
| `benchFps10` | 16 | `bench.c` | per-station result |
| everything else | ~340 | | sprite row masks, segment handles and frame counts, cache entries, `objectiveState`, window and font statics, runtime |

Sprite working set total (`drawWall` through `map` in the map): 14,800.

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

## Stack - 4,096 allocated, ~400 used

Last measured 2026-09-12. Deepest frame is `draw()`: `f_wallDepth[60]` 120 +
`spriteHits[8]` 80 + `markedSprites[8]` 16 + `wallhits[3]` 30 + `wallImpact`
10 + scalars, about 290 bytes, with under 100 more in `drawWall` /
`projectSprite` beneath it. `loadSprite()` at ~80 (`fileName[64]` +
`segName[16]`) is the next largest.

The unused ~3.6 KB is the cheapest reserve if DGROUP ever gets tight: the stack
is not part of DGROUP, but shrinking it and moving a table there is a single
edit in the project file.

## RAM disk - ~121,519 bytes

Assets are opened from `LOC::M:\IMG\...`, so if the game is installed on the
internal RAM disk its files are also RAM.

| File | Bytes |
| --- | ---: |
| `PSION3D.IMG` | 49,664 |
| 50 x `.spr` at 1,040 | 52,000 |
| 4 x `.map` (`map1`, `map97`-`map99`) | 19,855 |

Installing to an SSD (`A:` / `B:`) removes this from the budget entirely.

## Totals

| | Bytes | % of 512 KB |
| --- | ---: | ---: |
| Program image | 96,582 | 18.4% |
| Far segments | 55,760 | 10.6% |
| **Process** | **152,342** | **29.1%** |
| RAM disk | 121,519 | 23.2% |
| **Total** | **~267 KB** | **52.2%** |

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

Code has grown ~20 KB since 2026-09-12 and data ~4.7 KB, of which 1 KB was
the ninth sprite cache frame and most of the rest `menu.c`'s buffers and
`_CONST` strings. Features should keep landing as code and far data, not as
near arrays - but at this rate `_TEXT` is the next limit to watch.

## Rules of thumb

- **Before adding a global array, check DGROUP.** Run `.\tools\memcheck.bat`,
  or read `__bss_end` in the map relative to the DGROUP segment paragraph
  (e.g. `0BFB:B990` → 47,504).
- **Bulk data goes far.** Sprites, level prose and the mission index already
  do. A 128x128 map is 32,768 bytes against 18,032 free and cannot live in
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
