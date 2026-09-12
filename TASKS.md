# psion3d task list

The running list of planned work. Ideas live here until they are picked up; when
one is done, tick it and keep the notes so the next person knows what shipped.

Conventions and architecture rules are in `AGENTS.md` and `CLAUDE.md` - read
those before starting any of these. Anything touching the renderer is subject to
the measured frame budget in `CLAUDE.md`, so cost it by ablation, not by
prediction.

## Open

### 1. Menu system
- [ ] Front-end menu with options and level select.

`main()` in [psion3d.c](psion3d.c:220) currently goes straight from window
creation to `loadMap(1)` and `mainLoop()`, so there is no state above the game
loop. This needs a mode above `gameRunTicks` that owns menu / briefing / playing
and decides which one gets the frame. Level select depends on task 16 having more
than one map to select.

Open questions: whether the menu draws into the same 240x160 bitmap through
`bitmap.c` primitives, or uses WLIB text drawing straight into the window.

### 2. Mission briefings
- [ ] Per-level briefing screen shown before the level starts.

Needs a text source per level (a `.txt` beside `map<N>.map`, or a table compiled
in - note the near-data budget is the binding memory limit, so file-loaded text
is likely the better choice) and a text renderer that can wrap into the game
window. Sits between menu and gameplay in the mode machine from task 1.

### 3. Mission objectives (GoldenEye pattern)
- [ ] Per-level objective list; all must be complete to finish the level.

Needs: an objective type set (reach an exit, destroy a target, retrieve an item,
protect an NPC), per-level objective data, a completion check on the tick path,
and end-of-level handling. Touches [enemy.c](enemy.c), [pickup.c](pickup.c) and
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
The style-internal names (`drawBrickPanels`, `brickPattern`, `BRICK_BAND_COUNT`)
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
- [map/map2.map](map/map2.map) is a showcase corridor holding one run of every
  type. Nothing loads it - `main()` calls `loadMap(1)` - so change that line to
  see it, and delete the file when it has served its purpose.
- Costs were not measured on hardware. The types that add full-width spans
  (`SHELF`, `LIGHT`) are the ones to watch on the fps counter.

### 7. Automap
- [ ] In-game map of the areas the player has explored.

The visited-cell plumbing is already in place and unused: `MAP_MASK_MARKED`,
`isMarked`, `markCell` and `unmarkCell` are defined in
[game_map.h](game_map.h:88) and called from nowhere. Needs a mark on the
player's cell each tick and a view that draws marked cells - either a screen
reached from the menu (task 1), or a persistent panel in the unused right-hand
120x160 of the 480x160 LCD. Note task 8 wants that same region, so the two need
deciding together.

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

There is no exit cell type in `getCellEncoding()` and no completion path, so a
level runs until the player quits. Needs an exit encoding, a check that
objectives are satisfied before it opens, and a transition to the next level or
the debrief. Pairs directly with tasks 3 and 16.

Start and exit are settled as map data rather than wall types: task 6 spent all
sixteen ids and deliberately left none for them.

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
- [ ] Strafing and a menu/pause key. ~~A use key.~~

The use key is done: `KEY_USE` is bit 9 of the `keys` mask, read from
`kbScan[4] & 0x1` (the device spacebar) in [psion3d.c](psion3d.c), and space or
E on the PC host. It is edge triggered in `updatePlayer` - `updatePlayer` runs
once per catch-up tick against one input sample a frame, so a held key would
otherwise fire several times in a frame. Today it throws switches; doors still
open on approach on their own.

`KEY_11` through `KEY_16` in [psion3d.h](psion3d.h) are still defined and
unbound. Strafing and a pause/menu key (needed by task 1) want scancodes in
[psion3d.c](psion3d.c) and matching keys on the PC host.

### 14. Per-level asset loading
- [ ] Load only the sprites a level actually uses.

`loadMapData()` in [game_map.c](game_map.c:29) loads all ten sprite slots on
every level regardless of what the map contains. Each sprite holds a segment,
and near data (DGROUP) is the binding memory limit on this target, so decoration
sprites (task 4) and more levels (task 16) both push on this. Wants per-level
asset lists driven off the same `mapId` switch that picks the wall style.

### 15. Difficulty levels
- [ ] Selectable difficulty.

`enemystats_t` is a per-type table in [enemy.h](enemy.h), so scaling enemy
health, damage and accuracy is cheap to apply at level load. The menu (task 1)
is the natural home for the setting. Since the combat pass (task 22) the
table also carries stagger threshold, wind-up, burst interval and reposition
chance - accuracy and damage are the two to scale for difficulty, since the
others define the archetype rather than the threat. The tuned baseline and
what each number does are in [BALANCE.md](BALANCE.md).

### 16. More levels
- [ ] Levels beyond `map/map1.map`.

There is one map today. `loadMap(mapId)` already formats
`LOC::M:\IMG\MAP\map<N>.map` and `loadMapData()` switches wall style on `mapId`,
so adding a level is: author the ASCII map, add the style case, ship the file.
Level select (task 1) has nothing to select from until this happens, and it is
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

Not done, and worth doing when task 20 wants them: a directory of golden
frames for known positions, diffed after each build; and a scripted key
sequence (`--keys "fwd:32,fire:1"`) so movement and collision can be checked
the same way - today only fire and use can be held, and only for the whole
loop.

### 18. DGROUP budget check
- [ ] Script that reads `PSION3D.MAP` and prints DGROUP used, failing above a threshold.

The recipe is at the end of [MEMORY_BUDGET.md](MEMORY_BUDGET.md): `__bss_end`
relative to the DGROUP segment paragraph. A PowerShell script that parses the
map after a build and prints used / free / limit, with a non-zero exit above
a configurable line (say 48 KB of the 64 KB), turns the memory rule of thumb
into a gate that runs after every build. Should also append a row to the
history table in `MEMORY_BUDGET.md` on request, so the table stops being
hand-maintained.

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
- [ ] A single command that builds, hashes, checks DGROUP, and builds the PC host.

Runs the DOSBox build, reports errors from `build.log`, hashes `PSION3D.IMG`
and compares to the previous run (unchanged = refactor-safe, changed = expected
for features), runs the DGROUP check (task 18), and builds the PC target. Once
task 17 exists, also renders the golden frames and diffs them. Intended to be
wired up as a project skill under `.claude/` so it runs after every change
without being asked. Note the DOSBox mount is pinned to this directory, so
only one build can run at a time; the script should fail fast if `build.log`
is locked rather than queue.

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
