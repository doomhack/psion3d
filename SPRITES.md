# Sprites

How the game's sprites work as of 2026-10-01: the file format, where frames
live in memory, how the ray cast finds them, and how a frame is scaled, clipped
and drawn into the two bitplanes. Everything is in [sprite.c](sprite.c) and
[sprite.h](sprite.h) unless noted; slot ids are in [sprslot.h](sprslot.h), and
the collection and draw calls are in `draw()` in [draw.c](draw.c).

Costs quoted are device measurements from the benchmark and its profiling build
(TASKS.md tasks 22 and 26). Sprites are now the largest cost wherever they are
on screen: 32.9 ms of the Enemies station's 60 ms frame, 86.7 ms of the Crowd
station's 116.

## The pipeline in one view

```
 PNG (<= 64x64)
   | tools\convert_sprite.bat
   v
 base<N>.spr  -- 16 byte header + 1,024 byte 2bpp frame, one file a frame
   | loadSprite(base, slot), at mission start (loadMapData)
   v
 far segment SPR<slot>  -- every frame of the slot, 1 KB each
   | getSpriteFrame(): LRU, 9 frames
   v
 near cache spriteCache[]  -- what the decoders read
   |
   |   draw(): the ray cast marks sprite cells it passes through,
   |           projectSprite() turns each into a screen column, height, depth
   v
 drawProjectedSprite()  -- clip, map columns, decode rows to masks, blit
 drawSprite()           -- the weapon overlay, 1:1, no scaling
   v
 screenBm  (black plane, then grey plane)
```

## Storage format

### Pixels

A frame is 64 x 64 pixels at 2 bits a pixel:

| Value | Meaning | Drawn as |
| --- | --- | --- |
| 0 | transparent | nothing - the background stays |
| 1 | grey | grey plane set, black plane cleared |
| 2 | black | black plane set, grey plane cleared |
| 3 | white | both planes cleared - an opaque pixel of LCD background |

The black plane sits over the grey one (CLAUDE.md, "Two bitplanes"), so a
sprite never needs "both set": black is drawn as black alone. White is the only
way for a sprite to hide what is behind it without darkening it.

Rows are 16 bytes, four pixels a byte, **low bits first**: pixel `x` of a row
is bits `(x & 3) * 2` of byte `x >> 2`. The frame is row-major, 1,024 bytes.

### The `.spr` file

Exactly 1,040 bytes; `loadSprite` rejects any other length (it checks that the
read after the payload returns `E_FILE_EOF`).

| Offset | Bytes | Field |
| --- | --- | --- |
| 0 | 4 | `'S' 'P' 'R'` and format version `1` |
| 4 | 1 | `left` - first opaque column, a multiple of 4 |
| 5 | 1 | `top` - first opaque row |
| 6 | 1 | `right` - one past the last opaque column, a multiple of 4 |
| 7 | 1 | `bottom` - one past the last opaque row |
| 8 | 8 | `bands` - one byte per 8-row band (rows 0-7, 8-15, ...) |
| 16 | 1,024 | the frame |

The bounding box is in pixels but measured in whole 4-pixel groups, so
`left` and `right` always land on a source byte boundary. Each band byte holds
the first and last opaque 4-pixel group in those eight rows, as
`(firstGroup << 4) | lastGroup` with groups 0-15; a band with nothing opaque is
`0xF0` (first 15 after last 0), which the drawing code reads as empty. The box
lets the draw skip transparent margins of the whole frame; the bands let it
skip them row by row, which matters for figures much narrower at the head than
at the shoulders.

A frame with nothing opaque has a box of all zeros, and `right == 0` is what
every caller tests for "draw nothing".

### Making one

`tools\convert_sprite.bat` turns a PNG of at most 64 x 64 into a frame. A
smaller image is centred in the 64 x 64 frame (rounded down, left and up).
Colours map as:

| Source pixel | Value |
| --- | --- |
| alpha 0, or pure red `FF0000` | 0 transparent - red is a keying colour |
| pure green `00FF00` | 3 white, forced - for white that must not be keyed out |
| R, G and B all >= 240 | 3 white, or 0 with `-WhiteTransparent true` |
| R, G and B all <= 32 | 2 black |
| anything else | 1 grey |

Raw output (`/f`, or `-OutputPath`) is the `.spr`; without it the script prints
a C array of the same bytes, which nothing uses any more. Do not redirect the
raw output with PowerShell's `>`, which re-encodes it. The files go in `spr\`
on the PC and `M:\IMG\SPR\` on the device.

### Frames and file names

A sprite is up to eight frames, one file each: `base0.spr` to `base7.spr`.
`loadSprite` takes frame 0 and then consecutive frames until one is missing, so
a sprite with four frames is `base0` to `base3` and the gap ends it.

## Slots, frames and sprite ids

A slot is one sprite - a base name and its frames. There are 32 slots
(`SPRITE_SLOT_CAPACITY`), 11 used:

| Slot | Name | Base | Frames | What |
| --- | --- | --- | ---: | --- |
| 0 | `SPRITE_SLOT_CIV` | `sci` | 8 | civilian (scientist) |
| 1 | `SPRITE_SLOT_MER` | `mer` | 8 | mercenary |
| 2 | `SPRITE_SLOT_SGR` | `sgr` | 8 | soldier |
| 3 | `SPRITE_SLOT_HVY` | `hvy` | 8 | heavy |
| 4 | `SPRITE_SLOT_PARTICLES` | `hit` | 2 | impact marks: 0 on a wall, 1 on a body |
| 5 | `SPRITE_SLOT_PICKUPS` | `pup` | 4 | pickups |
| 6 | `SPRITE_SLOT_DECORATIONS` | `dec` | 4 | set dressing (camera, computer desk, ...) |
| 7-10 | `PISTOL` `SMG` `AR` `LMG` | `ppk2` `mp5` `ak` `m249` | 2 each | the player's weapon: 0 held, 1 firing |

Everything that draws a sprite names a frame with one byte, the **sprite id**:
`(slot << 3) | frame`. An enemy's eight frames are its states
(`ENEMY_FRAME_*` in [enemy.h](enemy.h)): idle, two walk frames, aim, shoot,
hurt, dying and dead. A frame number past the slot's frame count wraps
(`frameNum -= frameCount` until it fits), so asking a two-frame sprite for
frame 5 draws frame 1 rather than nothing.

Pickups and decorations share one map cell layout (TASKS.md task 4): a sprite
cell's 4-bit type is 0-7 for a pickup and 8-15 for a decoration
(`DECOR_TYPE_BIT`), and the low three bits are the frame within
`SPRITE_SLOT_PICKUPS` or `SPRITE_SLOT_DECORATIONS`.

## Loading

`loadMapData()` in [game_map.c](game_map.c) calls `loadSprite(base, slot)` for
all eleven slots when a mission starts. Each call:

1. Closes the slot's segment if an earlier mission left one, and drops its
   frames from the cache. Each slot's segment is named `SPR<slot>`, so reusing
   a name without closing the old one would leak a segment a mission.
2. Opens `LOC::M:\IMG\SPR\<base><n>.spr` for n = 0, 1, ... and validates each:
   the header magic and version, a box inside 64 x 64, exactly 1,024 bytes of
   frame and then end of file. The box and bands go straight into
   `spriteFrameBounds[slot][frame]`.
3. Creates one far segment of exactly the frames found
   (`p_sgcreate(..., E_SEGMENT_HIGH)`, 64 paragraphs a frame).
4. Opens every file a second time and copies its frame into the segment
   through the 1 KB `spriteLoadBuffer`.

A frame that fails validation fails the whole slot; a slot that fails to load
has no segment and a frame count of 0, and `getSpriteFrame` then reports every
frame empty, so it **draws nothing** rather than a placeholder. There is
deliberately no built-in fallback sprite: one cost 2 KB of near data
(MEMORY_BUDGET.md).

The far segments are why art is cheap and near data is not: 50 frames are
51,200 bytes of far memory and none of DGROUP.

## The frame cache

The decoders read frames from near memory, so frames are copied out of their
far segments on demand into `spriteCache`, **nine** 1 KB slots managed least
recently used (`getSpriteFrame`):

- A hit is a scan of nine entries for the id, and a touch of a 16-bit use
  clock. When the clock wraps every entry's age is flattened to 0, so LRU
  order survives the wrap approximately.
- A miss evicts the least recently used entry (or an empty one) and copies 1 KB
  with `p_sgcopyfr`. That copy is the cost a miss carries - bringing far memory
  near measured about 0.35 ms per KB.
- The box and bands are not in the cache: they stay in
  `spriteFrameBounds`, near, for all 32 x 8 possible frames, so a frame's
  geometry is known without touching its pixels.

Nine is a measured number. The Decorations benchmark station shows nine
distinct frames at once (four decorations, four pickups, the weapon); with
eight slots, LRU one short of a cyclic working set missed on every access and
cost 3.2 ms a frame. A room that shows ten distinct frames will thrash the same
way, and every enemy in view in a different state is a different frame.

## Finding sprites: the ray cast

Sprites are found by the walls' ray cast, not by a separate pass over a list.
`draw()` casts 60 rays, one per 4-pixel column, and the DDA walk
([ddaasm.a](ddaasm.a)) stops at every cell that holds an unmarked sprite as
well as at walls. At such a stop, if the cell is not solid:

1. The cell is **marked** (`MAP_MASK_MARKED`) and remembered, so the rays that
   follow pass it - one sprite per cell however many rays cross it. The marks
   are cleared at the end of `draw()`.
2. An enemy cell (`MAP_MASK_ENEMY`) is projected at the enemy's own position
   (`enemy->x`, `enemy->y`), which moves smoothly within and between cells; its
   sprite id is the enemy type's slot and its current frame, mirrored for the
   walk frames when the enemy faces the other way.
3. Any other sprite cell - a pickup or a decoration - is projected at the
   cell's centre, and keeps the cell for the shot code.

Walls that can be seen past (windows, bars, arches) are not solid, so a sprite
standing in a doorway or behind bars is found too.

Two limits come with this. At most **eight** sprites are collected a frame
(`MAX_VISIBLE_SPRITES`, for marks and projections alike): a ninth in view is
not drawn and cannot be shot. And there is **no depth sort**: sprites are drawn
in the reverse of the order they were found. Along any one ray the nearer cell
is found first and so drawn last, which is right; across rays the order is by
screen column, so two sprites that overlap on screen but were found by
different rays can be painted in the wrong order.

### Projection

`projectSprite(x, y, ...)` transforms the world position into the view:

- `depth` is the distance along the view direction (perpendicular, not along a
  ray), `side` the distance across it, two `fpmul`s each.
- Anything nearer than `SPRITE_NEAR_DEPTH` (32, an eighth of a cell) or further
  than `SPRITE_FAR_DEPTH` (3,072, 12 cells) is rejected, and so is anything
  with `|side| >= depth` - outside a 90-degree cone, well past the 60-degree
  view, and the guard that keeps the next step's `fpmul` from wrapping.
- The screen column is `spanX = 30 + side / depth * 52`, in 4-pixel columns
  0-59. This is the routine's one `fpdiv`.
- The height is `30720 / depth` (`SPRITE_HEIGHT_NUM`), the same numerator walls
  use: a sprite frame is as tall as a wall at the same distance, so its 64 rows
  are the 2 metres of a cell. Sprites are square, so the width is the height.

A projected sprite is a `spritehit_t`: height, depth, column, an optional pixel
offset (only the impact marker uses it), the sprite id, mirror flag, and the
enemy id or the cell.

Before drawing, `draw()` drops any sprite whose **centre column** is behind the
wall there (`f_spriteDist >= f_wallDepth[spanX]`). That one test also decides
whether the sprite can be shot.

## Drawing a projected sprite

`drawProjectedSprite(hit, f_wallDepth)` does the rest. In order:

### 1. Place and clip

The sprite is centred on its column (`spanX * 4 + 2`) and on screen row 80, the
horizon, plus its offsets. The rectangle is clipped to the 240 x 160 view, then
shrunk to the frame's opaque box scaled to this size (`scaleBound`, a rounded-up
16-bit divide). A mirrored sprite has its box and bands mirrored first.

### 2. Clip against walls, column by column

The centre test above decides *whether* a sprite is drawn; this decides *which
of its columns*. Walking in from each end, columns whose wall is nearer than the
sprite are dropped, narrowing the span to the outermost columns it is in front
of. If a nearer wall also cuts through the middle - a window frame, a pillar -
`spriteColVisible` gets a 4-bit nibble per column, set where the sprite is in
front, and every decoded row is masked with it. Hidden columns are therefore
neither decoded nor drawn over the wall. The comparison is the sprite's
perpendicular depth against the wall's distance *along the ray*, which reads up
to 15% far at the screen edge (TASKS.md task 22 has the fix, not yet done).

### 3. Build the per-row spans

Each 8-row band of the source gives a destination x range from its band byte,
scaled to this size and clipped to the span. Rows of a band with nothing opaque
are skipped outright.

### 4. Map destination columns to source columns, once

Every row samples the same source column at a given screen column, so the
mapping is built once for the sprite by stepping a fixed-point accumulator
(`SPRITE_SCALE_BITS` = 8 fractional bits) across the span, backwards when
mirrored. What is stored depends on the decoder:

- Small sprites: `spriteCol[x]` is the source byte and `spriteColShift[x]` the
  bit shift of the pixel within it.
- Magnified sprites: `spriteCol[x]` is the source pixel column, with columns
  outside the sprite set to 64, an index that always reads transparent.

### 5. Walk the rows

A second accumulator steps the source row. A destination row is only decoded
when its source row differs from the last one decoded; a sprite drawn larger
than 64 rows repeats source rows, and those repeats only re-blit the masks
already built. Decoding produces three mask bytes per destination byte -
**opaque**, **black** and **grey** - in `spriteRowOpaque`, `spriteRowBlack` and
`spriteRowGrey`.

### 6. Decode - two decoders, split at 64 rows

Measured per size, neither decoder wins everywhere (TASKS.md task 22), so the
height picks one:

- **Below 64 (`buildSpriteRowMasks`)**: one pixel at a time - read the source
  byte, shift out two bits, branch on transparent / black / grey and set a bit
  in the masks. About 11 us a pixel.
- **64 and over (`buildMagnifiedRowMasks`)**: the source pixels the span covers
  are first unpacked to one byte each (`spriteRowPix`); then each destination
  byte gathers its eight pixels' 2-bit values into a word in the source's own
  packed layout, and three 256-entry tables (`spriteOpaqueMask`,
  `spriteBlackMask`, `spriteGreyMask`, built once, four pixels a lookup) turn
  each half into mask nibbles. About 4.4 us a pixel, but ~37 us a row and
  ~10 us a source byte of unpacking, which only rows as wide as a magnified
  sprite's pay back. The end bytes are masked to the span afterwards.

### 7. Blit

For each destination byte of the row, both planes through one pointer (the
grey plane is `BM_BYTES` past the black one):

- opaque mask `0xFF`: both plane bytes are stored outright, no read;
- opaque mask 0: skipped;
- otherwise: `plane = (plane & ~opaque) | colour` on each plane, a read and a
  write.

So a transparent pixel leaves the background alone, a white one clears both
planes, and black or grey set their own plane and clear the other.

## The weapon overlay

`drawSprite(spanX, y, spriteId)` draws the player's weapon, last in `draw()`
before the crosshair: frame 0 of the current weapon's slot normally, frame 1
while firing, at the weapon's `spanX * 4` and `y` from the weapon table in
[player.c](player.c), lowered by the switch animation and the recoil. It is
never scaled, so it has its own 1:1 path: rows clipped by the box and the
bands, then two source bytes (eight pixels) at a time into one destination byte
through the same three mask tables, with the same whole-byte fast path, and
half-byte cases at row ends that are 4- but not 8-pixel aligned. It costs a
flat ~4.8 ms a frame.

## Other users

- **Impact marks** (`drawImpact` in draw.c): a hit is held in world
  coordinates for three frames and projected each frame like any sprite,
  using frame 0 or 1 of the particles slot. Its pixel offsets are fractions of
  the target's on-screen height, and beyond `IMPACT_FAR_DEPTH` (two cells) its
  own size is held at 60 rows so it stays visible.
- **Cheats** (`cheatSprites` in draw.c): Mirror Mode flips every sprite and
  the projection's side; Tiny Enemies halves an enemy's height and drops it to
  stand on the same floor line.
- Tracers are lines, not sprites, but read the projected enemies' columns.

## Memory

| Where | Bytes | What |
| --- | ---: | --- |
| far | 51,200 | 50 frames in 11 segments |
| DGROUP | 9,216 | `spriteCache`, nine frames |
| DGROUP | 3,072 | `spriteFrameBounds`, 32 slots x 8 frames x 12 bytes - 11 slots used |
| DGROUP | 1,024 | `spriteLoadBuffer`, used only while loading |
| DGROUP | 768 | the three 256-entry mask tables |
| DGROUP | ~650 | column map, shift, unpacked row, row masks, `spriteColVisible` |

About 14.7 KB of near data in all, the second largest user after the screen
buffer (MEMORY_BUDGET.md). Each extra cache slot is another 1 KB of it.

## Cost

Measured on the benchmark's sprite stations (profile, 2026-09-30), with what
they draw a frame (counted on the PC host):

| Station | Sprites | Rows | Pixels drawn | Bytes written | Sprite ms | per pixel |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Enemies | 6 | 142 | 2,115 | 315 | 32.9 | 15.6 us |
| Decorations | 8 | 117 | 2,946 | 358 | 40.4 | 13.7 us |
| Crowd | 8 | 294 | 9,967 | 1,188 | 86.7 | 8.7 us |

The weapon is 1,316 pixels at 1:1 for 4.8 ms. Crowd is cheapest per pixel
because its screen-filling heavy goes through the magnified decoder and repeats
source rows.

What the work itself needs - stepping the interpolants, reading pixels, writing
them, nothing else - was priced on 2026-10-01 from a cost model calibrated on
this device (TASKS.md task 22): about 2.3 us a pixel, so 7.1 ms for Enemies,
8.9 for Decorations, 21.9 for Crowd and 1.1 for the weapon, 4-4.7x under
today's. The bus alone would be 0.2-0.6 ms, so sprites are bound by
instructions, not memory. That floor assumes a source format of one byte a
pixel (4 KB a frame) and a weapon stored ready-built as planes, both memory the
current format does not spend.

## Limits and gotchas

- **Eight sprites a frame**, found in ray order; the ninth is neither drawn
  nor shootable.
- **No depth sort**: overlapping sprites found by different rays can be drawn
  in the wrong order.
- **Nine cache frames**: more distinct frames than that on screen thrash, a
  1 KB far copy per miss.
- **Occlusion depth mismatch** at the screen edges (perpendicular sprite depth
  against along-ray wall distance), up to 15%.
- **Frame numbers wrap** modulo the slot's frame count instead of failing.
- **Loading opens every file twice**, once to validate and count, once to copy.
- **A missing or malformed file blanks its whole slot**, silently on the
  device (the PC host's `-v` reports failed opens).
- **21 slots are free**; filling them is far memory only, but each slot's
  bounds already reserve their 96 bytes of near data.

Rejected before, with figures in CLAUDE.md and TASKS.md task 22: a colour-run
(RLE) source format, in C and in assembler, and an assembler rewrite of the
same decoder loop.
