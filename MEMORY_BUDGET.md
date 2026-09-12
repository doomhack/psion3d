# Memory budget

Measured 2026-09-12 from `PSION3D.MAP`, working tree at `e64a92f` plus
uncommitted `pickup.c` / `player.c` changes. Re-measure with the recipe at the
end whenever a global array is added; the numbers here go stale, the shape of
the budget does not.

## The three limits

The build is `#model small jpi`: near code and near data are separate 64 KB
segments, and the SIBO machine has 512 KB total. Only one of the three is close.

| Limit | Used | Of | Headroom |
| --- | ---: | ---: | ---: |
| Near code (`_TEXT`) | 24,634 | 65,536 (37.6%) | 40,902 |
| **Near data (DGROUP)** | **42,816** | **65,536 (65.3%)** | **22,720** |
| System RAM (process + RAM disk) | ~201 KB | 512 KB (39%) | ~310 KB less OS |

**DGROUP is the binding constraint.** Every global, `static` and string
literal lands there. The 512 KB figure is not a useful ceiling for anything but
the sprite asset count.

The linker map lays code, stack and DGROUP out linearly and the total is over
64 KB (`0x11786`); that is fine because `CS` and `DS` are separate selectors.
Do not read the image total as a segment limit.

## Program image — 71,558 bytes resident

| Segment | Class | Bytes | What it is |
| --- | --- | ---: | --- |
| `_TEXT` | CODE | 24,634 | all C and assembler modules plus PLIB/WLIB stubs |
| `STACK` | STACK | 4,096 | see stack below; ~90% unused |
| `_MAGIC` | MAGIC | 4,096 | SIBO process control area, fixed by the runtime |
| `_INIT` | DATA | 2 | |
| `_CONST` | DATA | 2,434 | `sincos_tab` 2,048, `rayIdxOffset` 120, `enemyStats`, `weapons`, string literals |
| `_DATA` | DATA | 134 | initialised globals: `blackBm`/`greyBm`, bitmap masks, `player`, RNG seeds, `impact`, `tracers` |
| `_BSS` | BSS | 36,144 | zero-initialised globals — the real cost, broken down below |
| `BSS_END` | BSS | 6 | |

DGROUP is `_MAGIC` through `BSS_END`: 4,096 + 2 + 2,434 + 134 + 36,144 + 6
plus alignment = 42,816.

The `.IMG` file (27,280 bytes) is `_TEXT` + `_CONST` + `_DATA` + headers.
`STACK`, `_MAGIC` and `_BSS` are allocated at load and cost nothing on disk.

## `_BSS` — 36,144 bytes

The linker publishes only extern symbols, so file-scope `static`s appear as
gaps between consecutive publics. Regions below are those gaps, attributed from
the declarations.

| Region | Bytes | Owner | Notes |
| --- | ---: | --- | --- |
| `screenBm` | 10,240 | `bitmap.c` | both bitplanes, 320 rows x 32 B. Load-bearing, see CLAUDE.md |
| `map[64][64]` | 8,192 | `game_map.c` | packed `u16` cells |
| `spriteCache` | 8,192 | `sprite.c` | 8-frame near-RAM LRU; keeps far sprite count decoupled from DGROUP |
| `spriteFrameBounds` | 3,072 | `sprite.c` | 32 slots x 8 frames x 12 B — **11 slots used, ~2 KB reclaimable** |
| `recipTab` | 2,048 | `draw.c` | `rayDelta()` per trig entry, hoists a divide out of the ray loop |
| `enemyList` | 1,792 | `enemy.c` | 64 x 28 B `enemy_t`. Grew 20 → 28 B with AI memory fields |
| `spriteLoadBuffer` | 1,024 | `sprite.c` | file staging, used only during `loadSprite` |
| sprite masks | 768 | `sprite.c` | three 256-entry LUTs |
| `spriteColByte` / `ColShift` | 480 | `sprite.c` | per-column blit LUTs |
| everything else | ~340 | | row masks, segment handles, cache entries, `dbgTxt`, window statics, runtime |

Sprite working set total (`drawWall` through `map` in the map): 13,804.

## Far segments — 49,152 bytes

`loadSprite()` allocates one `p_sgcreate(E_SEGMENT_HIGH)` segment per slot,
sized to the frames actually found (1,024 B each, 64 paragraphs). These cost
**nothing in DGROUP**, which is why adding art is cheap and adding globals is not.

| Slot | Base | Frames | Bytes |
| --- | --- | ---: | ---: |
| `CIV` / `MER` / `SGR` / `HVY` | `sci` `mer` `sgr` `hvy` | 8 each | 32,768 |
| `PISTOL` / `SMG` / `AR` / `LMG` | `ppk2` `mp5` `ak` `m249` | 2 each | 8,192 |
| `PICKUPS` | `pup` | 4 | 4,096 |
| `PARTICLES` | `hit` | 2 | 2,048 |
| `DECORATIONS` | `dec` | 2 | 2,048 |
| **11 of 32 slots** | | **48** | **49,152** |

Full capacity is 32 slots x 8 frames = 256 KB, which would on its own be half
the machine. Nothing enforces a ceiling; keep an eye on it as slots fill.

`DIRECT_VIDEO_MEM_ACCESS` is defined in `psion3d.c`. The compatibility blit
path it disables would add one 10,240-byte segment-backed WLIB bitmap.

## Stack — 4,096 allocated, ~400 used

Deepest frame is `draw()`: `f_wallDepth[60]` 120 + `spriteHits[8]` 80 +
`markedSprites[8]` 16 + `wallhits[3]` 30 + `wallImpact` 10 + scalars, about
290 bytes, with under 100 more in `drawWall` / `projectSprite` beneath it.
`loadSprite()` at ~80 (`fileName[64]` + `segName[16]`) is the next largest.

The unused ~3.6 KB is the cheapest reserve if DGROUP ever gets tight: the stack
is not part of DGROUP, but shrinking it and moving a table there is a single
edit in the project file.

## RAM disk — ~85,648 bytes

Assets are opened from `LOC::M:\IMG\...`, so if the game is installed on the
internal RAM disk its files are also RAM.

| File | Bytes |
| --- | ---: |
| `PSION3D.IMG` | 27,280 |
| 48 x `.spr` at 1,040 | 49,920 |
| 2 x `.map` at 4,224 | 8,448 |

Installing to an SSD (`A:` / `B:`) removes this from the budget entirely.

## Totals

| | Bytes | % of 512 KB |
| --- | ---: | ---: |
| Program image | 71,558 | 13.7% |
| Far sprite segments | 49,152 | 9.4% |
| **Process** | **120,710** | **23.0%** |
| RAM disk | 85,648 | 16.3% |
| **Total** | **~206 KB** | **39.4%** |

The OS, window server and file system take their own share on top.

## History

| Date | `_TEXT` | DGROUP | Far | Note |
| --- | ---: | ---: | ---: | --- |
| 2026-09-05 | 15,468 | 44,272 | 35,840 | baseline |
| 2026-09-05 | 15,038 | 42,214 | 35,840 | test-pattern fallback removed: −2,058 DGROUP |
| 2026-09-11 | 22,760 | 42,400 | 49,152 | pickups, decorations, tracers, `gameloop.c` |
| 2026-09-12 | 24,634 | 42,816 | 49,152 | AI memory fields, player collision, hit feedback |

Code has grown ~9 KB in a week; data has barely moved. That is the intended
shape — features should land as code and far data, not as near arrays.

## Rules of thumb

- **Before adding a global array, check DGROUP.** `__bss_end` in the map,
  relative to the DGROUP segment paragraph (e.g. `0704:A740` → 42,816).
- **Bulk data goes far.** Sprites already do. A 128x128 map is 32,768 bytes
  against 22,720 free and cannot live in `map[][]` as declared; it would need
  a far segment and accessor changes in `game_map.h`.
- **`{0}` initialisers move a variable from `_BSS` to `_DATA`.** Same DGROUP
  cost, but it is then stored in the `.IMG` too. Leave large zeroed arrays
  uninitialised.
- **String literals are near data.** Every `drawDbgText("...")` format costs
  its length in `_CONST`.
- **Do not re-add a near-data fallback sprite.** The test pattern cost 2 KB of
  DGROUP and both `getSpriteFrame` callers already handle a missing slot.

## How to re-measure

Build, then read the segment table at the top of `PSION3D.MAP`:

```bash
"C:\Program Files (x86)\DOSBox-0.74-3\DOSBox.exe" -c "D:" -c "tsc /m unnamed.pr /smain=psion3d /v0 /zq > D:\build.log" -c "exit"
```

```bash
sed -n '3,12p' PSION3D.MAP
```

DGROUP used is the `__bss_end` offset. To attribute a gap between two publics,
list the `static`s in the modules linked between them — link order follows
`unnamed.pr`. Far segment total is frames loaded in `loadMapData()` x 1,024.
