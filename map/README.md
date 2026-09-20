# Level file format

`map<N>.map` is a plain text file, LF or CRLF, edited in any text editor. It
holds the 64x64 grid plus everything the game needs to know about the level:
where the player starts and finishes, what the mission is called, and the
briefing text. `loadMap()` in [game_map.c](../game_map.c) reads it in one pass
with an 80-byte stage buffer; the grid goes into `map[][]`, the numbers into
`mapInfo`, and all the prose into a far segment reached through
`mapTextCopy()`, so a level's text costs no near memory.

## Layout

```
[MAP]
XXXXXXXX...   64 rows of 64 cells

[LEVEL]
start    = 27, 1        ; cell the player spawns in, 0-based x, y
angle    = 90           ; compass bearing: 0 north (up the grid), 90 east, 180 south, 270 west
end      = 23, 16       ; cell that ends the level
title    = Cold Storage
location = Hamburg, Germany
mappos   = 130, 44      ; pixel position on the world map screen

[BRIEFING]
Free text, 100-200 words. Consecutive lines are joined with a space, so
wrap them however you like; a blank line starts a new paragraph.

[OBJECTIVE]
Locate the laboratory                          ; first line is the title
Find the hidden lab in the basement below the cold store.   ; the rest is its briefing

[OBJECTIVE]
...between two and five in total
```

## Rules

- A section header is `[NAME]` at the start of a line; the name is not
  case-sensitive. The four sections are `MAP`, `LEVEL`, `BRIEFING` and
  `OBJECTIVE`, and `OBJECTIVE` may repeat up to five times
  (`MAP_MAX_OBJECTIVES`). Anything else is an error.
- The file is read as if it began with `[MAP]`, so the old headerless grid
  files still load. Write the header anyway.
- `[MAP]` is exactly 64 rows of 64 cells, as before: every byte that is not a
  line ending is a cell, so no trailing spaces there. The cell characters are
  decoded by `getCellEncoding()` in [game_map.c](../game_map.c).
- A line starting with `;` is a comment anywhere, and blank lines are
  skipped. Outside `[MAP]`, leading and trailing whitespace is dropped too.
- `[LEVEL]` lines are `key = value`. A `;` after the value starts a comment,
  so a title cannot contain one. Unknown keys are an error, so a typo fails
  the load rather than silently dropping a field.
- `start` and `end` are `x, y` cell coordinates, 0-based, `x` the column and
  `y` the row, the same numbering as `map[y][x]` and the PC host's `--pos`
  readout divided by 256. Map 1's spawn is `27, 1`.
- `angle` is a compass bearing in degrees: 0 faces up the grid (north), 90
  right (east), 180 down (south), 270 left (west). The loader converts it to
  the engine's angle, whose 0 is east and which turns towards the bottom of
  the grid; 90 in the file is engine angle 0, 180 is 402.
- `mappos` is a pixel position for the world map screen; the units are that
  screen's and nothing else reads it.
- Text lines may not begin with `[` (it would be read as a header). Indent
  with a space if you must.
- The briefing and objective text are stored as strings with `'\n'` between
  paragraphs; the renderer word-wraps them. The total text per level,
  including terminators, must fit `MAP_TEXT_BYTES` (3072).

## Defaults

A file with no `[LEVEL]` section spawns at `27, 1` facing east, with the end
cell and map position at `0, 0`, no title or location, and no objectives.
Missing text items read as empty strings through `mapTextCopy()`.

## Starting a new level

Copy `template.map` to `map<N>.map` and edit it. It is a solid grid with an
8 x 6 room at the top left, every `[LEVEL]` key filled with a placeholder, and
the cell character legend in comments at the top. Only files named `map<N>.map`
are ever loaded, so the template itself is inert.

## Checking a file

The PC host reports the reason for a failed load on stderr, with the line
number, and `-v` prints everything it parsed:

```
psion3d_pc --map 1 -v
```

The menus read this file: Mission Select lists every `map<N>.map` from 1
upward until the first missing number (title and location per entry, and
`mappos` marks the world map), the briefing screen shows `[BRIEFING]`, and
the objectives screens list the `[OBJECTIVE]` titles and briefs.

What the cells *do* on a given level is not in the file. Picking up an item,
using or shooting a decoration sprite, using a switch and killing an enemy each
report the cell to the level's handler in [level.c](../level.c), which is where
a keycard is tied to one door or a computer to an objective; a level with no
handler gets the defaults (keycard and switch open every locked door, the rest
do nothing). Level completion is still open in [TASKS.md](../TASKS.md)
(task 10).
