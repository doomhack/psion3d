# Memory budget

Measured 2026-10-01 from `PSION3D.MAP`, branch `sprite_optimisation` (task 26
and the sprite rewrite: the direct sprite draw and the weapon blit in
`sprasm.a`, then `spriteFrameBounds` dropped). Re-measure with the recipe at the end whenever a global array is
added; the numbers here go stale, the shape of the budget does not. The
history table is appended by `memcheck -Record` on every change and is the
current figure when it disagrees with the tables above it - refresh those when
it does.

## The three limits

The build is `#model small jpi`: near code and near data are separate 64 KB
segments, and the SIBO machine has 512 KB total.

| Limit | Used | Of | Headroom |
| --- | ---: | ---: | ---: |
| Near code (`_TEXT`) | 48,708 | 65,536 (74.3%) | 16,828 |
| **Near data (DGROUP)** | **43,984** | **65,536 (67.1%)** | **21,552** |
| System RAM (process + RAM disk) | ~275 KB | 512 KB (54%) | ~237 KB less OS |

**DGROUP is the binding constraint.** Every global, `static` and string
literal lands there, and `memcheck` fails the build check above 48 KB - 5,168
bytes away, the most room since task 19. The sprite rewrite gave back 3,632
of it: 720 when the three 256-byte mask tables and the decoders' scratch
buffers went for 576 bytes of per-row span tables in the frame cache, then
2,912 when `spriteFrameBounds` (3,072, a box and eight bands for every frame
that could ever load) gave way to the box travelling in the frame.

**Code is the fuller segment by share** (74.3% against 67.1%) and the one that
grows. Of `bmasm.a`'s 3,510 bytes, 3,200 are the unrolled row tables of the
three wall spans (640 + 640 + 1,920), bought for ~11 ms a frame on the
wall-heavy benchmark stations; `sprasm.a`'s two 8-pixel blocks are 432 more.
Unrolling anything else is a code decision as much as a speed one. The 512 KB
figure is not a useful ceiling for anything but the sprite asset count.

The linker map lays code, stack and DGROUP out linearly and the total is over
64 KB (`0x17A28`); that is fine because `CS` and `DS` are separate selectors.
Do not read the image total as a segment limit.

## Program image - 96,808 bytes resident

| Segment | Class | Bytes | What it is |
| --- | --- | ---: | --- |
| `_TEXT` | CODE | 48,708 | all C and assembler modules plus PLIB/WLIB stubs, by module below |
| `STACK` | STACK | 4,096 | see stack below |
| `_MAGIC` | MAGIC | 4,096 | SIBO process control area, fixed by the runtime |
| `_INIT` | DATA | 2 | |
| `_CONST` | DATA | 4,249 | `sincos_tab` 2,048, `enemyStats`, `ammoTypes`, `weapons`, `ddaWalkers` 8, and ~1,800 of string literals and const tables (menu labels, cheat names, HUD text) |
| `_DATA` | DATA | 386 | initialised globals: `rayIdxOffset` 120, `player`, `blackBm`/`greyBm`, bitmap masks, RNG seeds, mode/mission/settings/bench/cheat state |
| `_BSS` | BSS | 35,243 | zero-initialised globals - the real cost, broken down below |
| `BSS_END` | BSS | 8 | |

DGROUP is `_MAGIC` through `BSS_END`: 4,096 + 2 + 4,249 + 386 + 35,243 + 8
plus alignment = 43,984.

The `.IMG` file (53,424 bytes) is `_TEXT` + `_CONST` + `_DATA` + headers.
`STACK`, `_MAGIC` and `_BSS` are allocated at load and cost nothing on disk.

The profiling build (`PROF3D.IMG`, `tools\profile.bat`) is not this image: its
DGROUP is 44,304, +320 for the pass tables, 4,848 to the limit. Keep it inside
the limit too.

## `_TEXT` by module - 48,708 bytes

Only extern functions are published, so a module is measured from its lowest
public to the next module's; a module's leading `static` functions are counted
in the one before it. Treat the figures as +/- a couple of hundred bytes.
`psion3d.c` publishes only `main`, so its 754 bytes of statics sit after the
last assembler module, `sprasm.a`, and are taken out of it here.

| Module | Bytes | | Module | Bytes |
| --- | ---: | --- | --- | ---: |
| `menu.c` | 6,633 | | `player.c` | 1,865 |
| `walls.c` | 5,985 | | `sprite.c` | 1,601 |
| `draw.c` | 5,923 | | `mission.c` | 1,287 |
| `enemy.c` | 4,584 | | `sprasm.a` | ~1,318 |
| `bmasm.a` | 3,510 | | `ui_psion.c` | 1,203 |
| `game_map.c` | 3,233 | | `labwall.c` | 1,150 |
| `bitmap.c` | 2,229 | | `psion3d.c` + `fpasm.a` | ~812 |
| `gameloop.c` | 2,215 | | `cheat.c` | 583 |
| `hud.c` | 1,925 | | `automap.c` + `bench.c` | 477 |
| SDK start-up and stubs, `N$` helpers | 1,335 | | `pickup.c` + `decor.c` + `level.c` | 424 |
| `ddaasm.a` + `videomem.a` | 416 | | | |

Grouped: renderer (bitmap, draw, walls, labwall, sprite, and the assembler
`bmasm`, `ddaasm`, `sprasm`, `videomem`) ~22.3 KB; game (enemy, player, map,
gameloop, pickups) ~12.3 KB; front end (menu, mission, hud, automap, bench,
cheat, ui) ~12.1 KB; runtime and `main` ~2.2 KB. `debug.c`, `fp_math.c` and
`settings.c` contribute data only. The sprite rewrite took `sprite.c` from
3,270 to 1,759 (both decoders, the column tables and the old 1:1 loop went)
for `sprasm.a`'s ~1,318: -186 net; dropping `spriteFrameBounds` and the band
mirroring took it to 1,601.

## `_BSS` - 35,243 bytes

The linker publishes only extern symbols, so file-scope `static`s appear as
gaps between consecutive publics. Regions below are those gaps, attributed from
the declarations.

| Region | Bytes | Owner | Notes |
| --- | ---: | --- | --- |
| `screenBm` | 10,240 | `bitmap.c` | both bitplanes, 320 rows x 32 B. Load-bearing, see CLAUDE.md |
| `spriteCache` | 9,936 | `sprite.c` | 9-frame near-RAM LRU, 1,104 B a frame (pixels, per-row span bytes and the box); keeps far sprite count decoupled from DGROUP. Nine because the Decorations benchmark station cycles nine frames, and LRU one short of the working set misses every access (3.2 ms) |
| `map[64][64]` | 8,192 | `game_map.c` | packed `u16` cells |
| `recipTab` | 2,048 | `draw.c` | `rayDelta()` per trig entry, hoists a divide out of the ray loop |
| `enemyList` | 1,792 | `enemy.c` | 64 x 28 B `enemy_t` |
| `menuText` and line table | 1,504 | `menu.c` | 1,280 byte copy of the briefing or objective text being shown, 48 line starts/lengths, two 40 byte strings |
| `spriteLoadBuffer` | 1,104 | `sprite.c` | a frame as it will be stored - pixels, the span bytes built after them, the header's box - used only during `loadSprite` |
| `spriteRows` | 42 | `sprite.c` | the block `sprasm.a`'s two row loops read by absolute address (sprasm.h) |
| `mapInfo` | ~88 | `game_map.c` | level numbers and text offsets, 8 benchmark stations |
| `benchFps10` | 16 | `bench.c` | per-station result |
| everything else | ~360 | | `spriteColVisible` (30, the per-column wall clip), segment handles and frame counts, cache entries, `objectiveState`, window and font statics, runtime |

Sprite working set total (`drawWall` through `map` in the map): 11,262, down
3,633 with the sprite rewrite - the three 256-entry mask tables (768), the
decoders' column tables and row buffers (635) and `spriteFrameBounds` (3,072)
went, for 720 of span bytes and boxes in the cache, 80 in the load buffer and
the 42 byte `spriteRows` block. Near sprite data no longer grows with the
slot count: a frame's geometry is only near while the frame is cached. The
profiling build adds `passFrames` / `passTicks` (2 x 128) and `benchSkip` to
`bench.c`.

## Far segments - 59,760 bytes

`loadSprite()` allocates one `p_sgcreate(E_SEGMENT_HIGH)` segment per slot,
sized to the frames actually found: 1,104 B each (69 paragraphs), the file's
1,024 bytes of pixels, the 64 span bytes `buildRowSpans` makes from them, and
a paragraph holding the header's 4-byte box, so a cache fill brings all three
in one copy. These cost **nothing in DGROUP**, which is why
adding art is cheap and adding globals is not.

| Segment | Base | Frames | Bytes |
| --- | --- | ---: | ---: |
| `CIV` / `MER` / `SGR` / `HVY` | `sci` `mer` `sgr` `hvy` | 8 each | 35,328 |
| `PISTOL` / `SMG` / `AR` / `LMG` | `ppk2` `mp5` `ak` `m249` | 2 each | 8,832 |
| `PICKUPS` | `pup` | 4 | 4,416 |
| `DECORATIONS` | `dec` | 4 | 4,416 |
| `PARTICLES` | `hit` | 2 | 2,208 |
| **11 of 32 sprite slots** | | **50** | **55,200** |
| `MAPTXT` | `game_map.c` level prose | | 3,072 |
| `MISIDX` | `mission.c` 20 missions x 74 B, with best times | | 1,488 |

The history table's Far column, and `memcheck`, count sprites only (memcheck
at 1,104 a frame since 2026-10-01; earlier rows are at 1,088 and, before the
sprite rewrite, 1,024).

Full sprite capacity is 32 slots x 8 frames = ~276 KB, which would on its own
be over half the machine. Nothing enforces a ceiling; keep an eye on it as slots fill.

`DIRECT_VIDEO_MEM_ACCESS` is defined in `psion3d.c`. The compatibility blit
path it disables would add one 10,240-byte segment-backed WLIB bitmap.

## Stack - 4,096 allocated, ~500 used

Measured 2026-10-01 from the `sub sp` in each function's `tsda` listing.
Deepest is `draw()`: 374 bytes of locals (0x176) - `f_wallDepth[60]` 120,
`spriteHits[8]`, `markedSprites[8]` 16, `wallhits[3]` 42 since `wallhit_t`
went to 14 bytes, the `ddaray_t` block 10, the hoisted per-frame values and
scalars - plus five saved registers, ~390 bytes, up from ~290. Under it
`drawProjectedSprite` takes 72 + 10 (92 before the sprite rewrite; 34 of it is
the `groupX` table `spriteDrawRows` fills), and `sprasm.a`, the wall styles
and the `ddaWalk` walkers a few words each, so ~480 at the deepest. `loadMapFile()` at 226 (its 64 byte
read chunk and parser state) and `loadSprite()` at 94 are the next largest,
and never under `draw()`.

The unused ~3.6 KB is the cheapest reserve if DGROUP ever gets tight: the stack
is not part of DGROUP, but shrinking it and moving a table there is a single
edit in the project file.

## RAM disk - ~125,413 bytes

Assets are opened from `LOC::M:\IMG\...`, so if the game is installed on the
internal RAM disk its files are also RAM.

| File | Bytes |
| --- | ---: |
| `PSION3D.IMG` | 53,424 |
| 50 x `.spr` at 1,040 | 52,000 |
| 4 x `.map` (`map1`, `map97`-`map99`) | 19,989 |

Installing to an SSD (`A:` / `B:`) removes this from the budget entirely.

## Totals

| | Bytes | % of 512 KB |
| --- | ---: | ---: |
| Program image | 96,808 | 18.5% |
| Far segments | 59,760 | 11.4% |
| **Process** | **156,568** | **29.9%** |
| RAM disk | 125,413 | 23.9% |
| **Total** | **~275 KB** | **53.8%** |

The sprite rewrite moved memory out of DGROUP and into the far heap: -3,968
resident image, +4,000 of span bytes and boxes across the 50 frames in their
segments.
The `.spr` files are unchanged; the spans are built at load.

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
| 2026-10-01 | 48,866 | 46,896 | 54,400 | branch `sprite_optimisation`: direct scaled draw and weapon blit in `sprasm.a`, decoders and mask tables gone, frames carry 64 span bytes (Far now at 1,088 a frame) |
| 2026-10-01 | 48,708 | 43,984 | 55,200 | `spriteFrameBounds` dropped: the box travels in the frame (1,104 B, 69 paragraphs), the unread bands gone |

Code has grown ~24 KB since 2026-09-12 and data ~4.8 KB, of which 1 KB was
the ninth sprite cache frame and most of the rest `menu.c`'s buffers and
`_CONST` strings. Task 26 added 4.1 KB of code and 8 bytes of data for a
15.3 -> 22.9 fps benchmark: the trade the budget wants, speed bought in the
segment with room. Features should keep landing as code and far data, not as
near arrays - but `_TEXT` is now the fuller segment by share, with 16 KB left.

## Rules of thumb

- **Before adding a global array, check DGROUP.** Run `.\tools\memcheck.bat`,
  or read `__bss_end` in the map relative to the DGROUP segment paragraph
  (e.g. `0CE5:ABD0` → 43,984).
- **Bulk data goes far.** Sprites, level prose and the mission index already
  do. A 128x128 map is 32,768 bytes against 21,552 free and cannot live in
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
Far sprite total is frames loaded in `loadMapData()` x 1,104 (pixels, span bytes and the box).
