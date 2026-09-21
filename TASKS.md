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
- [x] Options screen (2026-09-20): Sound 0-10 as the design's segment bar
      and Show FPS On/Off, edited with left/right, held in [settings.c](settings.c)
      (`soundLevel`, `showFps`). Nothing plays a sound yet (task 12); Show FPS
      gates the device debug window's fps line, and will gate the HUD's FPS
      row (the design's new HUD artboard) when there is a HUD. Not saved -
      every launch is 7 / Off - until there is a save file. Cheats stays off
      the main menu.
- [ ] Cheats screen, unlocks and the Cheat Unlocked dialog - task 24.
- [x] Mission outcome screens (complete / failed / killed in action), mission
      timer, best times - see tasks 9 and 10. Best times still want a save file.

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
  line under it (nothing in the map file holds them), and the Cheats entry
  on the main menu, left out until its screen exists rather than shown as a
  dead item (Options joined the menu on 2026-09-20).

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
`objectiveBriefOfs[]` and `mapTextCopy()`.

- [x] Level event callbacks (2026-09-20). [level.c](level.c) holds one
      handler per level, `levelEvent(event, item, x, y)`, selected in
      `loadMapData()` next to the wall style and `levelEventNone` for a level
      without one. Six events: `PICKUP` (from `collectPickup`), `USE_DECOR`
      and `USE_SWITCH` (from `tryUse`), `SHOOT_DECOR` (from
      `resolvePlayerShot`), `KILL` (from `damageEnemy`) and `EXIT` (from
      `updatePlayer`, once on entering `mapInfo.endX/endY` - the place for
      objectives that are only known to have held at the end). A handler returns
      TRUE to replace the default; scripts move objectives through
      `missionSetObjective(i, state)`, which pops "Objective N Completed" /
      "Failed" through `uiInfoMsg` (`wInfoMsg` on the device, stderr on the
      PC) on a real change and is silent otherwise. The only defaults are keycard and switch
      opening every locked door (`unlockDoors()`; `unlockDoor(x, y)` is the
      single-door version for scripts). Decorations now stop the player's
      round like an enemy does - marker on the sprite, nothing behind it hit
      - and `spritehit_t` carries the cell for that. Map 1's handler
      completes objective 2 on any computer (narrow to the director's desk
      once the map decides which), fails objective 3 on a civilian kill and
      completes it on exit if it has not failed.
      Four golden views cover the shot, the use, the kill and the exit; the
      pause-screen ones needed `--screen` to run after `--frames` in the PC
      host, which it now does. Not hooked: shooting a `SHOOTABLE` wall.
      Cost: `_levelEvent` is 2 bytes of DGROUP; every call is per event, not
      per frame.

- [x] Completion check and end-of-level handling (2026-09-20, task 10):
      `missionTick()` judges the objectives the tick the player stands on the
      end cell, after `LEVEL_EVENT_EXIT`, and opens the outcome screen.

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
  0.75, to find the wall being used; `SWITCH` reacts, and since the level
  event callbacks (task 3) a decoration sprite in reach does too. Both stop at the
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
- [x] On-screen health, ammo, weapon and objective status.

Done 2026-09-20 as the HUD from the design's HUD artboard: [hud.c](hud.c)
draws the two 120px side panels - Health (the number in the 16px face at
double height, `G_STY_DOUBLE`, over a ten-segment bar), Objectives done /
total, the FPS row when the option is on; Weapons 1-4 with the key box, name
and `ammo/cap` (a dash for the pistol, nothing for a weapon not yet picked
up), the current one inverse. It draws through `ui.h` into a new
`UI_TARGET_HUD`: on the device a full-screen window created first, so the
game window sits over its middle 240 columns and `blitVideoMem` writes the
view between the panels; on the PC an image the view is composited over, so
the window now shows the whole 480x160 LCD in play as well as in the menus
(`--hud` screenshots it; the bare 240 view stays what the gameplay goldens
compare). The old debug window is gone; `debug.c`'s `drawDbgText` has no
caller on the device now (the PC HUD label still shows the slot).

Redraw cost, measured 2026-09-21: the first version invalidated the whole
HUD window whenever a value moved and repainted both panels from a redraw
event - about 45 buffered calls plus the event round trip and the
server's background clear - and that read as a visible hitch once a second
with the fps counter on and 17 -> 9 fps while firing. Now the values are
*cells*: `hudUpdate()` runs once a frame with the HUD target selected,
compares each value with the one on screen, and replaces only the cells
that moved, drawn straight into the window (no invalidation, no redraw
event - the calls ride in the frame's client buffer). A cell is one
`uiTextBox` call, `gPrintBoxText` on the device: it clears or fills its
box, aligns and prints in a single message, so a shot costs exactly one
message for its ammo cell, a hit one for the number plus one or two segment
interiors, and the fps counter one per second. A weapon switch redraws its
two rows (about seven calls each). `hudDraw()` still paints everything and
is what a `WM_REDRAW` (the window uncovered after a menu) gets. Measured on
the device after the change: 17 -> 16 fps while firing, and that remaining
frame is the shot itself (the muzzle flash sprite and the hit test), not the
HUD.

### 9. Player death and game over
- [x] Handle `player.health` reaching zero.

Done 2026-09-20 with task 10. `missionTick()` in [mission.c](mission.c),
called per tick from `gameRunTicks`, sees health at zero, lets 16 ticks pass
so the last hit's flash and shove are seen (the dead player takes no input:
`updatePlayer` gets an empty key mask), then ends the mission as
`OUTCOME_KIA` and opens the outcome screen: Killed in Action, the
objectives as they stood, Enter to retry the level, Esc to Mission Select.
No fade; the menu window comes up over the last frame.

### 10. Level exit and completion
- [x] A way to finish a level.
- [x] Judge the mission on the end cell and show the outcome.
- [ ] Persist best times (they live in the far mission index and go with the
      process) - task 23.

Done 2026-09-20. `missionTick()` tests the player's cell against
`mapInfo.endX` / `endY` every tick, after `updatePlayer` has raised the level
script's `LEVEL_EVENT_EXIT` (so "still true at the end" objectives complete
first). Every objective complete is `OUTCOME_COMPLETE`, anything failed or
still open is `OUTCOME_FAILED`; a failure earlier in the mission does not end
it - the player plays on to the exit, as the objectives screen shows. The
outcome screen (`MENU_OUTCOME` in [menu.c](menu.c), from the design's
Outcome artboards) has the 28 row title bar with mission, difficulty, time
and best time, the objective list with the failed one picked out, and Enter
Continue (complete) or Enter Retry (failed / KIA); Esc is Mission Select
either way, so a mission always comes back to the list. The mission clock is
`missionTicks`; best times per mission and difficulty sit in the `MISIDX`
segment record, set on completion with the "new" flag the design shows -
this session only, until there is a save file. The device brings the menu
window up from inside `runTicks` when the mode changes under it; the frame
loop then spins without frames until the menu window's redraw event ends the
outstanding event request (see `mainLoop`). Golden views: `map1_exit` (the
failed outcome, from the end cell with objectives open) and `map1_kia`
(`--dead`, a new PC option that zeroes health). A complete outcome cannot be
reached headlessly until `--keys` (task 17) can play a level.

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

### 23. Save game
- [ ] Save to file: missions completed, cheats unlocked, best times, options.
- [ ] Reset Game item on the Options screen - wipes the save (with a
      confirm, like Abort) and returns everything to first-launch state.

Everything the game remembers today goes with the process: `soundLevel` /
`showFps` in [settings.c](settings.c) (task 1), `difficulty` in
[mission.c](mission.c) (task 15), and best times per mission and difficulty
in the `MISIDX` far segment record, set by `missionTick` (task 10). Mission
completion is not recorded anywhere yet, and cheats (task 24) have no state
to save until their screen exists - reserve the bits. Options should load
before the main menu draws, so `gameInit()` is the read and the Options
screen and the outcome screen are the writes; the file is small enough to
rewrite whole each time rather than patch. Keep the record out of DGROUP
(read straight into the mission index segment / the settings globals) and
version it with a magic + version header so a missing or malformed file is
a clean first launch, never a crash. Path goes beside the assets, e.g.
`LOC::M:\IMG\SAVE.DAT`; check `p_open` write modes and the `E_FILE_EOF`
convention from CLAUDE.md. Once it lands, tick the "persist" items in tasks
1, 10 and 15.

### 24. Cheats
- [ ] `u16 cheatFlags` (one bit per cheat), the hooks below, and a Cheats
      screen on the main menu that toggles the unlocked ones.
- [ ] Unlocks: each cheat is earned by completing a given mission at a given
      difficulty inside a time limit, GoldenEye style. Needs task 23 - an
      unlock that goes with the process is not worth showing.
- [ ] A cheated run records no best time and earns no unlock (`setBest` in
      [mission.c](mission.c) and the unlock check both test `cheatFlags`).
- [ ] Cheat Unlocked dialog on the mission outcome screen.

Sixteen cheats, chosen 2026-09-21 so that every one is a value change at
spawn time or a branch in per-tick code - nothing touches the per-ray or
per-column paths, so the frame budget is untouched and the near-data cost is
the one flag word. Where a cheat does touch the renderer it is noted, and it
still wants measuring on the device like anything else.

| Cheat | Hook |
| --- | --- |
| Invincibility | `hurtPlayer()` in [player.c](player.c) keeps the shove and the hurt flash, skips the health subtraction. |
| All weapons | `playerInit` gives every weapon, as picking up all four would. |
| Infinite ammo | `updatePlayer` skips the `ammo[ammoType]--` at the shot; the empty-pool check is then never true. |
| Turbo mode | `PLAYER_MOVE_TICK` / `PLAYER_TURN_TICK` scaled x1.5 or x2 where `updatePlayer` applies them. Knockback and impulse derive from the same constants - decide whether they scale too. |
| One-shot kills | Spawn-time: `enemyList[id].health = 1` in `getEnemyCell()` ([enemy.c](enemy.c)) instead of the archetype's value, so the stagger threshold falls out automatically. |
| Perfect aim | `getShotSpan()` returns 0 - no bullet spread for the player. |
| Rapid fire | `shootCooldown` set to 1 at the shot instead of `fireDelay`. More shot resolutions per second, but that is per-frame span work, not per-ray. |
| Invisibility | `enemyCanSeePlayer()` returns FALSE. Enemies still hear gunfire through `alertEnemies()` and search, they just never acquire. |
| Pacifist | Enemies never enter AIMING / ATTACKING; the chase-or-wander decision at the end of `runAI` treats every type as CIV. Distinct from Invisibility in the menu text: "enemies can't see you" versus "enemies never attack". |
| Slow enemies | `runAI()` on even ticks only from the catch-up loop in [gameloop.c](gameloop.c). Halves the AI cost as a side effect. Exclusive with Fast. |
| Fast enemies | `runAI()` twice per tick. Doubles AI cost, which measured as near-free; `enemyMoveTickTable` is consumed twice, which is the intent. Exclusive with Slow. |
| Mirror mode | Renderer, but not per-pixel: negate `rayIdxOffset[]` per column in [draw.c](draw.c) and the sprite screen-x projection in [sprite.c](sprite.c) (`spriteMirrored` already exists for the frames). The weapon impact span is symmetric about the centre so it needs nothing. Mirror the automap too or the pause map lies. |
| Tiny enemies | Halve `spriteHeight` in the projection (`SPRITE_HEIGHT_NUM / f_depth`). Fewer pixels filled, so cheaper than normal; hit resolution follows the projected span so shots still land. |
| Mercs | Spawn-time: the `switch(cell)` in `getEnemyCell()` maps E/F/G all to `ENEMY_TYPE_MER`. All four sprite sets load on every level ([game_map.c](game_map.c)) so there is no extra segment. Civilians stay civilians so Pacifist keeps meaning. Exclusive with Soldiers / Heavies. |
| Soldiers | As Mercs, to `ENEMY_TYPE_SGR`. |
| Heavies | As Mercs, to `ENEMY_TYPE_HVY`. |

Menu rules: Slow / Fast are one radio group and Mercs / Soldiers / Heavies
another - the last one toggled wins and clears the others. Rapid fire +
Infinite ammo is the combination that sells the feature; Rapid fire alone
empties the pistol pool in about two seconds, which is the point.

Considered and dropped for cost: X-ray (skipping the depth test draws every
collected sprite, ~4ms each), big heads (more fill per sprite), dual-wield (a
second ~4ms overlay), extra enemies, any full-screen effect (the blit is bare
`rep movsw`; anything per-word on top is the +28ms interleaving lesson), and
noclip (the DDA from inside a solid cell hits at distance 0 and
`30720 / distance` divides by zero). Free ones held back for a later batch:
full automap reveal, enemy positions on the automap, wide-angle lens, low
camera, instant weapon switch, one-hit `damageEnemy` as the alternative
one-shot.

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
- [x] A benchmark map of stations, run from Options, with a frame rate per room.

Done, as a dedicated map rather than the build-time define first proposed:
the "map 1 corridor" the old numbers were taken in no longer exists (the
spawn moved to (27,3) facing south in the rework), and real levels will keep
changing. [map/map97.map](map/map97.map) is the benchmark, past the mission
scan like the 98/99 showcases and in the lab style: seven rooms joined by
doors, one per rendering aspect - a corridor, a 12x12 empty room seen across
its diagonal (the DDA-heavy case), a corridor lined with the detail wall
types, a wall of doors, windows, bars, an arch and pillars, six idle enemies,
decorations and pickups, and a crowd with a heavy filling the screen. Each is
a `station = x, y, bearing, name` line in `[LEVEL]` (`MAP_MAX_STATIONS`
8, parsed into `mapInfo.stations`), so a new aspect is a room and a line.

Options > Benchmark ([bench.c](bench.c)) loads the map and stands at each
station for `BENCH_STATION_TICKS` (5 s): the tick loop is bypassed, so no
input, AI or mission clock, and the frame after each teleport is a warm-up
that starts the clock without being counted. The result is frames over ticks
in tenths, and the run ends on `MENU_BENCH` the way a mission ends on its
outcome - a results screen of `Corridor 19.7` rows with the run's average in
the title bar. Esc mid-run shows what was measured, Enter runs it again. The
whole thing costs 176 bytes of DGROUP.

On the PC host `--map 97 --station N` places the player as the run does and
`--bench --frames 1400` runs it under the virtual clock, where every station
reads 32.0; both are golden views (`bench_*`), so the scenes the numbers
come from are pinned. Fixing that made the headless `--frames` loop hold the
clock while it runs: the wall clock used to creep into long runs, and
`pcTickAdvance` counted double when paused.

Still to do on hardware: the first run's numbers, as the baseline for the
budget table in `CLAUDE.md`.

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
