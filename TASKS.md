# psion3d task list

The running list of planned work. Ideas live here until they are picked up; when
one is done, tick it and keep the notes so the next person knows what shipped.

Conventions and architecture rules are in `AGENTS.md` and `CLAUDE.md` - read
those before starting any of these. Anything touching the renderer is subject to
the measured frame budget in `CLAUDE.md`, so cost it by ablation, not by
prediction.

## Open

### 1. Menu system
- [x] Front-end menu with level select and a pause menu.
- [ ] Options screen (sound level - nothing to play yet, task 12).
- [ ] Cheats screen, unlocks and the Cheat Unlocked dialog.
- [ ] Mission outcome screens (complete / failed / killed in action), mission
      timer, best times - wait on tasks 3, 9 and 10 and a save file.

Done 2026-09-19 from the Design artifact (14 artboards at 480x160; the
screens that shipped are Main Menu, Mission Select, Mission Briefing, Mission
Objectives, Pause, Abort Confirm, Pause Objectives, Pause Map). How it is put
together:

- **Mode.** `gameMode` in [gameloop.c](gameloop.c) is `GAME_MODE_MENU` or
  `GAME_MODE_PLAYING`. `gameInit()` scans the missions and opens the main
  menu; `gameKey()` takes a `UI_KEY_*` event in either mode (Esc in play
  pauses) and returns whether to repaint or quit; `gameStartMission(mapId)`
  loads the level and switches to play. The platform watches `gameMode`
  change under a `gameKey` call, shows or hides the menu window, and
  restarts `gameTime` from the tick counter so the pause is not caught up.
- **Drawing.** The menus draw with WLIB into a third, full-screen 480x160
  window (`ui_psion.c`) through the eight primitives in [ui.h](ui.h), using
  the ROM Swiss 13 and Swiss 16 fonts the design's Helvetica sizes map to.
  [menu.c](menu.c) is portable and knows nothing of WLIB; the PC host has a
  QPainter implementation in `pc/src/menu_pc.cpp`, so every screen has a
  golden frame (`--screen <name>`), with the caveat that those frames are of
  Qt's Helvetica, not the ROM fonts. Menus are event driven on the device
  (`wGetEventWait`), so a menu on screen costs no CPU.
- **Input.** Play still reads scancode levels; menus and the in-game Esc are
  window server key events (`WM_KEY`), so no new scancodes were guessed.
  Esc is the pause key (task 13).
- **Missions.** [mission.c](mission.c) parses `map1.map` .. `map20.map` at
  startup (stopping at the first missing, which keeps 98 and 99 out) with
  the new `loadMapFile()` - `loadMap()` minus the sprite loading - and keeps
  title, location and `mappos` per mission in a far segment. So adding a
  level to the list is adding the file (task 16). `loadMapFile` reads the
  file in 64 byte chunks now; it was a `p_read` per byte.
- **Difficulty** (task 15): Agent / Senior / Elite on the select screen,
  applied in `enemyShootPlayer` to damage and accuracy (x3/4, x1, x5/4;
  first guesses, see BALANCE.md). Not saved anywhere yet.
- **Pause Map** (task 7): [automap.c](automap.c) renders 26 rows of the
  level at 4x4 pixels a cell into `screenBm` with the `bitmap.c` primitives
  and `uiBlitMap` copies it into the menu window (on the device through the
  same segment-backed bitmap the compatibility blit uses). The whole level
  is shown; the design has no unexplored state, and `isMarked()` is the
  one-line change if it ever wants one.
- **Objectives**: `objectiveState[]` in `mission.c` is reset at mission
  start and read by the pause screen; nothing sets it until task 3.
- **Near data**: `menu.c` holds a 1,280 byte text buffer for the briefing,
  the line table and two 40 byte strings, about 1.5 KB; the string literals
  are another ~600 bytes of `_CONST`. DGROUP is at 45,120 with 4 KB to the
  48 KB limit - see MEMORY_BUDGET.md.
- Not in this pass, by choice: the main menu clock, the world map ROM image
  (a dithered box with the `mappos` crosshair stands in), the coordinates
  line under it (nothing in the map file holds them), and the Cheats and
  Options entries on the main menu, which are left out until their screens
  exist rather than shown as dead items.

The three `lab_room` / `lab_pipes` / `lab_pillars` golden frames were
already failing at the commit before this work (checked by stashing) and
were left alone.

### 2. Mission briefings
- [x] Per-level briefing screen shown before the level starts.

Done with task 1: the briefing screen word-wraps the `[BRIEFING]` text (a
greedy layout in `menu.c` measuring with `uiTextWidth`) into five 19px lines
a page under the location line, with up/down scrolling and space paging.
Mission Select parses the chosen file with `loadMapFile()` before opening it,
so the text is the level's own; the assets load when the mission starts.

### 3. Mission objectives (GoldenEye pattern)
- [ ] Per-level objective list; all must be complete to finish the level.

The per-level data is done: up to five `[OBJECTIVE]` sections per map file,
each a title line and a briefing, reached through `mapInfo.objectiveOfs[]` /
`objectiveBriefOfs[]` and `mapTextCopy()`. Still needed: an objective type
set (reach an exit, destroy a target, retrieve an item, protect an NPC), a way
for an objective to point at a cell or enemy, a completion check on the tick
path, and end-of-level handling. Touches [enemy.c](enemy.c), [pickup.c](pickup.c) and
the map encoding - some objectives will want map cells or enemy ids to point at.
The briefing screen from task 2 is the natural place to list them.

### 4. Decoration sprites
- [x] Computer desks, security cameras, and similar set dressing.

Done. [decor.c](decor.c) mirrors `pickup.c`: map digits `'1'`..`'8'` are
decorations 0..7, drawn as frames `dec0`..`dec7` of `SPRITE_SLOT_DECORATIONS`.

| id | char | sprite |
| --- | --- | --- |
| 0 | `1` | camera |
| 1 | `2` | computer desk |
| 2-7 | `3`-`8` | reserved |

The cell is `MAP_MASK_SPRITE` plus a type, and nothing else: no wall bit, so
the ray runs past it and collects it like any sprite; no walk bit, so
`updatePlayer` and `enemyTryMoveTo` both refuse the cell. There is no spare
flag bit to tell a decoration from a pickup, so the type nibble does it: a
sprite slot holds eight frames, pickups are types 0..7 and decorations are
8..15 (`DECOR_TYPE_BIT`), and the sprite pass in [draw.c](draw.c) picks the
slot from bit 3 and the frame from the low three. Decorations count against
`MAX_VISIBLE_SPRITES` (8) like everything else, so a room full of them can
push an enemy out of the frame.

Not measured on hardware yet. Sprite cost is per visible sprite, so a corridor
lined with them is the case to put on the fps counter.

### 5. Generic wall type names
- [x] Rename `WALL_TYPE_BRICK` to a neutral default.

Done: the id is `WALL_TYPE_SOLID`. Every style renders it differently, so the
name now describes what the map means rather than what one style draws.
Verified as a pure rename - `PSION3D.IMG` hashed identically either side of it.
The style-internal names (`drawBrickPanels`, `brickPattern`, `BRICK_BAND_COUNT`; the lab ones have since become `drawConcretePanels` and `LAB_PANEL_*`)
were left alone; those genuinely describe one style's appearance.

### 6. Fill out the 16 wall types
- [x] Use the remaining ids in `MAP_BLOCK_TYPE_MASK` for visual variety.

Done. All sixteen ids are used. Six new appearance types, one new mechanic, and
id 5 changed meaning:

| id | char | name | flags | notes |
| --- | --- | --- | --- | --- |
| 5 | `S` | SHOOTABLE *(was SECRET)* | WALL SOLID | opens to floor when shot; no longer walk-through |
| 9 | `!` | SIGN | WALL SOLID | board, mural, screen, plaque |
| 10 | `*` | LIGHT | WALL SOLID | clears both planes for the one true highlight in the palette |
| 11 | `:` | PIPES | WALL SOLID | vertical runs; the cheapest type, one span at most per column |
| 12 | `#` | SHELF | WALL SOLID | racking; full-width rails, so the costly shape - held to two |
| 13 | `R` | LOW | WALL | parapet seen over; not solid, so the ray runs on |
| 14 | `\|` | PILLAR | WALL | post seen past; not solid |
| 15 | `U` | SWITCH | WALL SOLID | thrown with the use key; unlocks every locked door |

Characters avoid `= + - @` so cells stay safe to type in `Psion Levels.xlsx`.
`Q`, `Y`, `Z`, the digits `1`-`9` and all lowercase are still free for
decoration sprites (task 4).

Notes for whoever touches this next:

- `LOW` and `PILLAR` are the only two that are not solid, so the DDA walks past
  them and their columns pay an extra hit and an extra `drawWall` call. Use them
  for effect, not as corridor filler.
- `LOW` returns FALSE, so its column does not occlude. The depth buffer holds
  one distance per column and cannot say "solid below row 100", so a sprite
  behind a parapet draws over it. Returning TRUE instead would hide that sprite
  entirely in the open air above the wall, which is worse. The real fix is a
  foreground re-draw pass after the sprite loop.
- Two probes, both cheap because neither runs per frame. `hitWallCell` in
  [draw.c](draw.c) steps forward from `f_wallDepth` in sixteenths of a cell to
  find the wall a shot landed on, and only `SHOOTABLE` reacts. `tryUse` in
  [player.c](player.c) steps forward from the player in quarter cells, up to
  0.75, to find the wall being used, and only `SWITCH` reacts. Both stop at the
  first wall so they cannot reach through one.
- `unlockDoors()` moved out of `giveKeycard()` into
  [game_map.c](game_map.c); the keycard and the switch both call it.
- `WALL_TYPE_LOCKED_DOOR` had no case in [walls.c](walls.c) at all and drew as a
  blank occluding column. `drawWallD` is now `drawWallDoorGap` plus two
  wrappers, the same shape `labwall.c` already had.
- [map/map99.map](map/map99.map) is a showcase corridor holding one run of every
  type, numbered out of the way so `map2` onward are free for real levels.
  Nothing loads it on the device - `main()` calls `loadMap(1)` - so change
  that line to see it, or `psion3d_pc --map 99`. Two golden views in
  [golden/views.txt](golden/views.txt) render it, so keep the file: it is the
  only map that exercises every wall type.
- Costs were not measured on hardware. The types that add full-width spans
  (`SHELF`, `LIGHT`) are the ones to watch on the fps counter.

### 7. Automap
- [x] In-game map, as the pause menu's Map screen.
- [ ] Explored-only display, if wanted.

Done with task 1 as [automap.c](automap.c): a 26 row window of the whole
level, panned with up/down, with the player as an arrow along their facing.
Solid cells are black, see-through walls grey, walk-through walls a grey
ring, locked doors grey with a black core. The visited-cell plumbing
(`MAP_MASK_MARKED`, `isMarked`, `markCell`, `unmarkCell` in
[game_map.h](game_map.h)) is still unused: an explored-only map is a mark
on the player's cell each tick plus an `isMarked` test in `drawCell`. The
right-hand 120x160 of the LCD is still free for task 8.

### 8. Status bars
- [ ] On-screen health, ammo, weapon and objective status.

The player already carries `health`, `weaponsOwned`, `items`, current weapon
and, since task 11, `ammo[]` per pool in [player.h](player.h). Ammo is the
most pressing thing to show: today the only sign a pool is dry is the weapon
swapping itself for the pistol. The display question is where it goes: the game window
is the 240x160 region at x=120, the debug window is the 120x160 region at x=0,
and the right-hand 120x160 of the 480x160 LCD is unused. Putting status in a
side window costs no game-view rows and no per-frame raycast work, but the
status window is not part of the `blitVideoMem` path, so it needs its own draw
route and should only redraw when a value changes.

### 9. Player death and game over
- [ ] Handle `player.health` reaching zero.

`hurtPlayer` in [player.c](player.c) already clamps health to 0 (and applies
the knockback, view kick and hurt flash), and `enemyShootPlayer` in
[enemy.c](enemy.c) stops enemies firing at a dead player, but nothing else
reacts - the player keeps walking and shooting at 0 health. Needs a death state,
a death screen or fade, and a route back to the menu or a restart. Objectives
(task 3) mean little until failure is possible.

### 10. Level exit and completion
- [ ] A way to finish a level.

The exit cell is now map data: `end = x, y` in the file's `[LEVEL]` section lands
in `mapInfo.endX` / `endY` (the spawn is `start` and `angle` the same way, and
`initPlayer()` reads them). Nothing tests it yet, so a level still runs until
the player quits. Needs a check that the player's cell is the end cell and the
objectives are satisfied, and a transition to the next level or the debrief.
Pairs directly with tasks 3 and 16.

Start and exit are map data rather than wall types: task 6 spent all sixteen
ids and deliberately left none for them.

### 11. Ammunition
- [x] Track and consume ammo.

Done. The pistol is infinite (`AMMO_TYPE_NONE`); the SMG, AK47 and LMG each
draw from their own pool in `player.ammo[]`, sized by `ammoTypes[]` in
[player.c](player.c) - `{pickup, cap}` per pool, currently 10/90, 20/120,
30/160. There are no separate ammo pickups: a weapon pickup *is* that weapon's
ammo, so `giveWeapon` in [pickup.c](pickup.c) tops the pool up on every
collection and grants the weapon only on the first. Firing an empty weapon
switches to the pistol through the normal lower/raise, which is the only
"empty" signal until task 8 lands.

The pickup rule and the reasoning are in [BALANCE.md](BALANCE.md): 2x what it
costs to kill the enemy that drops the weapon, at that enemy's engagement
range. The numbers are first guesses; enemy mix sets the economy and there are
no levels yet (task 16), so expect them to move.

### 12. Sound
- [ ] Weapon, hit and alert sounds.

There is no audio anywhere in the codebase. The SIBO sound API and the frame
budget are both unknowns here, so this wants a spike first: make one sound play
without moving the fps counter, then decide how far to take it. Treat any cost
estimate as a guess until measured.

### 13. Control scheme
- [x] ~~A menu/pause key.~~ ~~A use key.~~ ~~Strafing.~~

The use key is done: `KEY_USE` is bit 9 of the `keys` mask, read from
`kbScan[4] & 0x1` (the device spacebar) in [psion3d.c](psion3d.c), and space or
E on the PC host. It is edge triggered in `updatePlayer` - `updatePlayer` runs
once per catch-up tick against one input sample a frame, so a held key would
otherwise fire several times in a frame. Today it throws switches; doors still
open on approach on their own.

Strafing is done: `KEY_STRAFE_LEFT` and `KEY_STRAFE_RIGHT` are bits 10 and 11,
read from `,` (`kbScan[3] & 0x2`) or A (`kbScan[6] & 0x4`) and `.`
(`kbScan[7] & 0x10`) or D (`kbScan[5] & 0x10`) on the device. W
(`kbScan[6] & 0x20`) and S (`kbScan[6] & 0x10`) double the up/down arrows, so
WASD moves and strafes while the arrows move and turn, on device and PC alike. [player.c](player.c) keeps a separate
`f_strafeVel` with the same impulse/damping model as `f_moveVel` but capped at
3 m/s against the 4 m/s walk, and combines both into one `tryMove` along the
facing and its right vector `(-sin, cos)`. Diagonal movement is deliberately
not normalised: forward plus strafe is a 3-4-5 triangle, 5 m/s or 1.25x
walking speed - the classic strafe-run. Keep it.

The pause key is Esc, and it is not a scancode at all: the play loop asks
the window server for key events (`WE_KEY`) and `W_KEY_ESCAPE` opens the
pause menu through `gameKey()` (task 1). The menus read all their keys the
same way. `KEY_13` through `KEY_16` in [psion3d.h](psion3d.h) are still
defined and unbound.

### 14. Per-level asset loading
- [ ] Load only the sprites a level actually uses.

`loadMapData()` in [game_map.c](game_map.c:29) loads all ten sprite slots on
every level regardless of what the map contains. Each sprite holds a segment,
and near data (DGROUP) is the binding memory limit on this target, so decoration
sprites (task 4) and more levels (task 16) both push on this. Wants per-level
asset lists driven off the same `mapId` switch that picks the wall style.

### 15. Difficulty levels
- [x] Selectable difficulty.
- [ ] Tune the multipliers by feel; persist the choice once there is a save file.

Done with task 1: Agent / Senior / Elite, chosen with left/right on Mission
Select, held in `difficulty` ([mission.c](mission.c)) and applied at the
shot in `enemyShootPlayer` through `difficultyDamage()` and
`difficultyAccuracy()` - x3/4, x1 and x5/4 on the two numbers BALANCE.md
names as the threat rather than the archetype. Senior is the tuned baseline
and the default; the other two are untested first guesses. The setting
resets to Senior every launch.

### 16. More levels
- [ ] Levels beyond `map/map1.map`.

There is one map today. `loadMap(mapId)` already formats
`LOC::M:\IMG\MAP\map<N>.map` and `loadMapData()` switches wall style on `mapId`,
so adding a level is: author the file to `map/README.md` (grid, `[LEVEL]`,
briefing, objectives), add the style case, ship the file. Mission Select
(task 1) lists `map1.map` upward until the first missing number, so a new
`map2.map` appears in the list with no code change; the showcase maps 98 and
99 stay out of it for the same reason.
Level select has one entry until this happens, and it is
the reason objectives (task 3) and the level exit (task 10) matter.
`Psion Levels.xlsx` in the repo root appears to be the level design workbook.

### 22. Combat balance and enemy AI
- [x] Weapon feel, enemy state machine, enemy mobility, tuning, hurt feedback.

Done across 2026-09-11/12, recorded here because it was never a list item and
the next person should know the ground moved. [BALANCE.md](BALANCE.md) is the
reference for all of it - numbers, formulas, and placement notes.

Shipped, roughly in order:

- **Weapon feel**: lower/raise on switch, recoil kick, impact markers that
  persist and land where the round struck, enemy-to-player tracers (XOR on the
  black plane so they read on any background). Weapon table settled by feel;
  sweet spots are SMG 1-2 cells, AK47 4-6, pistol 8, LMG everywhere.
- **AI structure**: every enemy used to be fully suppressed by any weapon (a
  hit restarted a 0.75s recovery, so nothing ever fired back). Now a flinch
  pauses an aim rather than resetting it, `staggerDamage` lets a Heavy shrug
  off SMG and AK rounds, fire is in bursts (`aimTicks` / `attackTicks` /
  `repositionChance`), evade is only rolled after a hit, a fleeing enemy stays
  fleeing when hit again, and only the Soldier retreats when the player closes.
- **Mobility**: enemies are an overlay on their cell (`underCell`), so they
  walk over pickups and through arches and doorways, see through everything
  the player can, and open doors by proximity (`doorEnemyNear` - the one bit
  of door state the game has). Corpses release their cell after ~1.3s.
- **Fairness**: `SEARCHING` re-checks sight every tick (it had a 4s blind
  window), and enemies remember an interrupted aim for 2s so micro-peeking
  does not reset them. The design rule is *no forced damage*: peek shorter
  than the wind-up, hide longer than the memory, and any room can be taken for
  zero. Verified by simulation for every weapon against every type.
- **Tuning**: all three combat types confirmed by feel. Danger anchored on
  GoldenEye Agent - one Merc ~100s to kill a passive player, Soldier ~60s,
  Heavy ~28s. `evadeChance` and `fleeChance` are still untouched by design.
- **Hurt feedback**: `hurtPlayer` owns damage and applies a shove away from
  the shooter (scaled by damage, allowed past walking speed), a view kick, and
  a black bar on the screen edge nearest the shooter for three frames. The
  player-enemy collision test is directional, so an overlap is always
  escapable.

None of the per-frame additions were measured by ablation on hardware. The
candidates, if the counter ever moves: the DDA sprite gate changed from
`!isWall` to `!isSolid` (one mask for another), `doorEnemyNear` per door
column, the tracer XOR lines, and the impact marker's three-frame life.

## Development infrastructure

Tooling that makes a change verifiable before it reaches the emulator. The
verification ladder today is: compiles (automated, 5s), bytes unchanged for a
refactor (automated, `PSION3D.IMG` hash), DGROUP within budget (manual recipe),
logic correct (PC build, by hand), looks right (emulator, by eye), performance
(device only). The items below automate the middle rungs so each session can
prove more without a human step, and turn the human steps into reading a
number instead of forming a judgement.

### 17. Headless PC build
- [x] Command-line mode that renders fixed frames to image files without a window.

Done. The full line is
`psion3d_pc --map N --pos X,Y --angle A --frames N --screenshot frame.png`;
the options are listed in [pc/README.md](pc/README.md). `--pos` and `--angle`
are Q8, the same numbers the HUD prints, so a view found by walking the window
build can be reproduced headlessly by typing its HUD readout back in. They go
through `hostSetPlayerPosition()` in [pc/src/host.c](pc/src/host.c), which
redraws so a `--screenshot` with no `--frames` shows the placed view, and
warns on stderr (without refusing) when the cell is not walkable. Either
option alone keeps the spawn's value for the other. `--fire` and `--use` hold
those keys through the `--frames` loop.

Checked: `--pos 7040,384 --angle 0` renders byte-identical to the default
spawn, and a bad value fails before any asset is loaded.

The golden frames landed with task 20 (`golden/`, diffed by
`tools\verify.bat`). Still not done: a scripted key sequence
(`--keys "fwd:32,fire:1"`) so movement and collision can be checked the same
way - today only fire and use can be held, and only for the whole loop.

### 18. DGROUP budget check
- [x] Script that reads `PSION3D.MAP` and prints DGROUP used, failing above a threshold.

Done. `.\tools\memcheck.bat` parses the linker map after a build and prints
`_TEXT`, DGROUP (used, % of 64 KB, bytes to the limit) and the far sprite
total, each with its delta against the last row of the history table in
[MEMORY_BUDGET.md](MEMORY_BUDGET.md), then the DGROUP segment breakdown
(`_CONST` / `_DATA` / `_BSS`). Exit 1 above the limit (`-Limit`, default
48 KB), exit 2 if the map is missing, so task 20 can gate on it.

To see what a change cost, copy `PSION3D.MAP` before it and run
`memcheck -Baseline old.MAP` after: the deltas move to the saved map and the
report adds every DGROUP region that changed size, attributed to the public
symbol it starts at (statics fold into the region after the preceding
public, as in the doc). `-Record "note"` appends today's row to the history
table in the file's own line endings.

Checked by adding a referenced 1,000-byte global: `_BSS` +1,000, DGROUP
+1,008 (paragraph alignment), the region named as `_memcheckProbe`. An
*unreferenced* global links to nothing and shows no delta, which is the
linker's smart linking, not a script bug.

### 19. Benchmark mode
- [ ] Build-time `#define` that spawns at the fixed measurement position and reports average fps.

Performance is measured on hardware only, and today that means standing in the
map 1 corridor and watching the once-a-second counter in
[psion3d.c](psion3d.c:157) for ten seconds. A `BENCH` define that forces the
spawn position and angle to the documented corridor spot, ignores input, and
prints the average over 10 seconds (or 200 frames) to a debug slot makes the
reading repeatable and the device step a number to copy into a commit
message. The position and angle should be written down once here so the
numbers in `CLAUDE.md` stay comparable across sessions.

### 20. One-shot verify script
- [x] A single command that builds, hashes, checks DGROUP, and builds the PC host.

Done. `.\tools\verify.bat` runs the ladder in about 15 seconds and prints one
`[OK]` / `[FAIL]` / `[SKIP]` / `[WARN]` line per rung: DOSBox build with
`Error` and `Warning` lines read back from `build.log` (a missing
`PSION3D.EXE` also counts as failure, since `tsc` deletes it), `PSION3D.IMG`
hash against the build before this one (`unchanged - refactor-safe` or
`CHANGED`), `memcheck` with the previous `PSION3D.MAP` as `-Baseline` so a
grown region is named, CMake build of the PC host (configured on first run
from the recipe in `pc/README.md`), and the golden frames. Exit 0 pass, 1
fail, 3 already running: one instance at a time, held by an exclusive lock on
`%TEMP%\psion3d-verify.lock` and a share-check on `build.log`, failing fast
as asked. Working files live in `.verify/` (gitignored).

Golden frames are the piece task 17 left open. [golden/views.txt](golden/views.txt)
lists a name and `psion3d_pc` arguments per view; the script renders each
headlessly and compares pixels with `golden/<name>.png`, reporting the count
and bounding box of any difference and leaving the rendered frame in
`.verify/frames/`. `-UpdateGolden` accepts the rendered frames. Four views
ship: map 1 spawn (lab style), map 1 two ticks into a shot (muzzle flash and
impact marker), and two of the `map99` showcase corridor (default style, every
wall type). Renders are deterministic - checked with 200 ticks of AI and with
fire held, twice each.

Wired up as the `/verify` skill in [.claude/skills/verify/SKILL.md](.claude/skills/verify/SKILL.md),
and `CLAUDE.md` now says to run it after every change.

Checked end to end: `WALL_DETAIL_DEPTH` 6 -> 2 plus a referenced 500-byte
global gave image CHANGED, DGROUP +512 naming `_memcheckProbe`, and all four
frames failing with the difference boxed; a syntax error gave the `tsc` line
and skipped the rest; reverting gave unchanged and four matches. Note
`-SkipPc` still diffs frames against the existing PC exe, so they are stale
after a portable-module change.

Still open from task 17: a scripted key sequence so movement and collision
can be golden-framed too.

### 21. Map validator
- [ ] Check a `.map` for size, unknown characters, spawn count and reachability.

Promoted from the housekeeping note below. Hand-authored ASCII with no
checking is fine for one map and not for several (task 16). Reachability from
the spawn cell over `MAP_MASK_WALK` cells is the useful part: it catches sealed
rooms and enemies placed inside walls, which otherwise only show up on device.
`getCellEncoding()` in [game_map.c](game_map.c) is the source of truth for
valid characters; the validator should reuse it via the PC build rather than
keep a second table.

## Housekeeping

- [ ] `TEXWALL.OBJ` is left over from the removed textured-wall experiment;
      there is no `texwall.c` and no `#compile texwall` in `unnamed.pr`. Delete
      the stale object.
- [ ] `~$Psion Levels.xlsx` is an Excel lock file that has been committed once
      already and is currently untracked. Add `~$*` to `.gitignore`.
- [ ] Map authoring is hand-written ASCII with no validator (now task 21). Before authoring
      several levels (task 16), consider a tool that checks a `.map` for size,
      unknown characters and unreachable cells - `Psion Levels.xlsx` suggests
      the design already happens in a spreadsheet, so a converter may fit.

## Notes

- No automated tests. Verification is a build plus running `PSION3D.IMG` in an
  emulator or on device.
- Adding a `.c` file means adding `#compile <name>` to `unnamed.pr`, and to
  `GAME_SOURCES` in `pc/CMakeLists.txt` if the module is portable.
- `psion3d.c` is the platform layer and is not built by the PC host, which
  supplies its own equivalents around `gameRunTicks`. New game logic - menus,
  briefings, objectives - belongs in a portable module or the PC build loses it.
- Near data (DGROUP) is the binding memory limit, not the 512K system total.
  Features that add tables or buffers should say where they sit.
