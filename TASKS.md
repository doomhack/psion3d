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
- [ ] Cheats screen, unlocks and the Cheat Unlocked dialog - task 24. The
      screen and all sixteen cheats are in; unlocks and the dialog wait on
      the save file.
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
- **Difficulty** (task 15): Recruit / Agent / Elite on the select screen,
  applied in `enemyShootPlayer` to damage and accuracy (x1, x4/3, x5/3;
  Recruit is the tuned table, the other two first guesses, see BALANCE.md). Not saved anywhere yet.
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
- [ ] Pitch enemy sounds up under the Tiny Enemies cheat (`cheatActive &
      CHEAT_TINY_ENEMIES`, task 24). How depends on what the spike finds the
      SIBO sound API can do: a playback rate or pitch parameter if there is
      one, otherwise resampled copies of the enemy sounds made at load time
      (a far segment, not DGROUP) or a second set of assets. Only enemy
      sounds; the player's weapons stay as they are.

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

Done with task 1: Recruit / Agent / Elite, chosen with left/right on Mission
Select, held in `difficulty` ([mission.c](mission.c)) and applied at the
shot in `enemyShootPlayer` through `difficultyDamage()` and
`difficultyAccuracy()` - x1, x4/3 and x5/3 (`difficultyScale[]`, in thirds)
on the two numbers BALANCE.md names as the threat rather than the archetype.
Recruit plays the `enemyStats[]` table as written, so BALANCE.md tunes
Recruit; Agent and Elite are untested first guesses scaled from it. Recruit
is the default, and the setting resets to it every launch.

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
- [x] `u16 cheatFlags` (one bit per cheat), the hooks below, and a Cheats
      screen on the main menu that toggles the unlocked ones.
- [ ] Unlocks: each cheat is earned by completing a given mission at a given
      difficulty inside a time limit, GoldenEye style. Needs task 23 - an
      unlock that goes with the process is not worth showing.
- [ ] A cheated run records no best time and earns no unlock (`setBest` in
      [mission.c](mission.c) and the unlock check both test `cheatFlags`).
      The best time half is done; the unlock check waits on unlocks.
- [ ] Cheat Unlocked dialog on the mission outcome screen.

The cheats themselves done 2026-09-22, all unlocked. How it is put together:

- **Module.** [cheat.c](cheat.c) owns `cheatFlags` (what the screen has
  on), `cheatActive` (what the mission in play runs with) and
  `cheatUnlocked` (`CHEAT_ALL` until unlocks exist; a locked row shows
  "Locked" and does not toggle). `gameStartMission` calls `cheatBegin`
  *before* `loadMap`, because the spawn-time cheats act as `getEnemyCell`
  places enemies. Every hook tests `cheatActive`, so nothing changes under
  a mission in progress, and it is zero on the benchmark map whatever is
  set - the benchmark measures the renderer as it ships, and the `bench_*`
  goldens stay put. The radio groups are `cheatToggle`'s job.
- **Screen.** Main menu -> Cheats (between Options and Exit): a five row
  scrolling list with the objective mark as the on box, the highlighted
  cheat's text on the right, Enter or Space toggles. A cheated mission's
  outcome screen says "cheats on" where a new best would say "new".
- **Hooks as built**, where they differ from the table: *All weapons* also
  gives one pickup of each ammo type, or the three guns start dry and fall
  back to the pistol at the first shot. *Turbo* scales the displacement and
  turn `updatePlayer` applies, not the constants, so the caps, impulses and
  damping keep their feel and a shove scales with the rest (x1.5, the
  largest step is then 96 Q8 plus the 38 radius, well inside a cell).
  *Pacifist* is `enemyActsCivilian()` in the four places the AI tested
  `ENEMY_TYPE_CIV`; the type itself is untouched, so a kill still reports
  the real type to the level script. *Slow* runs the AI on even `gameTime`.
  *Mirror* negates `rayIdxOffset` in place once per mission
  (`drawSetMirror`), so the ray loop is the same code reading the same
  table; `projectSprite` negates the side offset, a per-frame pass after
  the ray loop flips every sprite's frame, `cheatKeys` swaps left/right
  and strafe, the hurt flash swaps edge, and the automap flips. The weapon
  overlay is not flipped. *Tiny enemies* is the same post-loop pass:
  half height and width, `offsetY` keeping the feet on the floor line, and
  the shot test follows the narrower span. An impact marker on a tiny enemy
  is still placed at full-size scale, so it can land beside the body.
- **Checked** on the PC host with `--cheats <mask>` (new): 320 ticks among
  map 1's four mercs gave 90 health with nothing on, 100 under
  Invincibility, Invisibility, Pacifist and Slow, 40 under Fast, and two
  heavies on screen under All Heavies; one pistol round kills the civilian
  under One-shot. New goldens `menu_cheats`, `map1_mirror`, `map1_tiny`.
  Benchmark on the device 2026-09-22, cheats off (the benchmark map
  ignores them): Corridor 22.2, Empty room 18.6, Detail walls 21.1,
  Openings 14.2, Enemies 12.4, Decorations 11.4, Crowd 6.7, average 15.2 -
  against the task 22 build's 22.3 / 18.8 / 21.3 / 14.2 / 12.4 / 11.3 /
  6.7 (15.3), every station within the 0.3 noise band. The two wall-only
  stations both reading 0.2 down is worth a second run if it recurs
  (`draw()` gained a call after the ray loop and `rayIdxOffset` moved from
  `_CONST` to `_DATA`). The mirror and tiny passes *switched on* are
  unmeasured: that wants a fixed view on a mission map with the HUD fps.
- **Memory.** 912 bytes of DGROUP, almost all of it the names and texts in
  `_CONST`; they were cut to one short line each for that reason. DGROUP is
  47,504, 1,648 under the 48 KB gate.

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

### 25. Security cameras
- [ ] Cameras that watch for the player and raise an alarm after a grace time.
- [ ] HUD cue while a camera has the player in view.
- [ ] Shot cameras break and stop watching.
- [ ] `LEVEL_EVENT_ALARM`, so each level scripts what an alarm does.
- [ ] Camera objectives on a real level: "Destroy all cameras", "Do not trip
      the alarm".

Designed 2026-09-28. Cameras become gameplay rather than set dressing: being
seen for too long trips an alarm, and the player answers the cue by getting
out of view or shooting the camera before it fires.

**Agreed with the user:**

- **No facing.** Cameras go in room corners, so direction does not matter: a
  camera sees the player whenever the line from the camera cell to the
  player cell is open. Nothing to store in the cell, no cone maths.
- **Detection is not instant.** Being in view for longer than X seconds is
  the seen event. The player gets a cue on first sight and has that long to
  break line of sight or shoot the camera. This is the camera version of the
  enemy wind-up and keeps the *no forced damage* rule (task 22): careful play
  must always be able to avoid an alarm.

**Proposed, confirm by feel:**

- **Per-camera meter, drained rather than reset.** Each camera fills a meter
  one step per tick while it can see the player and drains while it cannot.
  Draining (at the fill rate, or slower) means repeated short peeks add up
  and a long enough hide clears it - the same shape as enemy aim memory. A
  hard reset on losing sight would allow endless peeking.
- **X by difficulty:** Recruit 3s (96 ticks), Agent 2s (64), Elite 1.25s
  (40). The floor is the time to notice the cue, find a camera that may be
  behind the player, turn, aim and fire one round - longer than the 0.5-1.0s
  enemy wind-ups because of the finding. `difficultyScale[]` multiplies, so
  this wants its own small table rather than the existing scaling.
- **Once per camera.** A camera that has fired goes quiet. The level script
  can count alarms if a second one should be worse.
- **Shooting cancels.** A hit clears the camera's meter the same tick.
- **Invisibility cheat blinds cameras** as it blinds enemies. The enemy
  speed cheats do not touch them.

**How it would be put together:**

- **Sight.** `enemyCanSeePlayer()` in [enemy.c](enemy.c) is a Bresenham walk
  from a cell to `playerCellX/Y` through `cellBlocksSight()`, and uses
  nothing from the enemy but its cell. Pull the body out as
  `canSeePlayerFrom(x, y)` and have the enemy version call it, so cameras see
  through exactly what enemies and the renderer do: windows, bars, arches,
  low walls, pillars and an open door. Expect the IMG to change on that
  refactor alone - TopSpeed's output moves on small source changes - so
  check the goldens hold instead.
- **Cameras see out of their room** through those same openings - a corner
  camera will catch the player in the corridor through a doorway. Either
  place cameras with that in mind, or add a Manhattan range cap
  (`CAMERA_RANGE`) that keeps them to their room and also skips the walk for
  far cameras. Decide when the first level is authored.
- **Cover.** Low walls and pillars do not block sight, so in a camera room
  only solid walls and the room's shape hide the player. Level design, not
  code.
- **Table.** A camera is decoration 0 (`1` in the map, task 4). Scan the map
  at load into a small table - x, y, meter, flags, four bytes each, capped at
  eight - so the tick does not search the grid. About 32 bytes of DGROUP plus
  the counter for the HUD; check against `MEMORY_BUDGET.md`.
- **Tick.** `cameraTick()` in a new portable `camera.c` (add it to
  `unnamed.pr` and `GAME_SOURCES`), called in `gameRunTicks()` after
  `runAI()` so `playerCellX/Y` are fresh. The benchmark branch returns before
  that loop, so cameras on `map97` stay frozen with no extra code. Cost is one
  Bresenham walk per camera in range per tick; it could run on alternate
  ticks, since the meter hides the delay.
- **Alarm.** A new `LEVEL_EVENT_ALARM` (item = camera index, x, y = camera
  cell) when a meter fills. The default, when the level's handler returns
  FALSE, is `alertEnemies()` at the player's cell - the camera reports where
  the player is, not where it is. Scripts can instead fail a stealth
  objective, lock doors (the reverse of `unlockDoor`), start an escape timer,
  or wake reinforcements. Reinforcements are cheapest as dormant enemies
  placed in a closed room and woken, rather than spawned.
- **Breaking.** Shooting a camera already raises `LEVEL_EVENT_SHOOT_DECOR`.
  Before the level event, mark the table entry dead and `updateCell` the cell
  to a broken-camera frame - reserve decoration 2 (`dec2`, map char `3`) for
  it, which also lets a map place pre-broken cameras. "Destroy all cameras"
  is then a script counting those hits against the camera count.
- **Cues.** Cameras see all round, so the player is often spotted by one
  behind them or off screen, and sound (task 12) does not exist yet - the
  cue has to be on the HUD. Show the fullest meter as one HUD cell, redrawn
  only when it crosses a step (eight steps, say), never by invalidating the
  window: per-cell draws measured 17 -> 16fps while firing, invalidation
  17 -> 9 (task 8). A second cue on the camera itself - swap its cell to a
  "tracking" frame while its meter is filling - tells the player *which*
  camera to shoot. That is an `updateCell` on a state change, not a per-frame
  cost, and wants one more reserved decoration frame.
- **Shooting it means being seen.** Sight is symmetric, so the player cannot
  shoot a camera it cannot see them from. X must cover stepping into the
  doorway and firing, or "destroy all cameras" and "never trip the alarm"
  cannot both be met on one level.

**Checking.** A PC host golden per state: camera in view with the HUD cue
part-filled, the broken frame, and the frame after the alarm (enemies
searching). The camera tick is per-tick game code, not renderer, but
measure on the device with the HUD fps from a fixed view in a camera room -
the benchmark cannot see it, since the world is frozen there.

**Follow-ups this opens** (separate tasks when picked up):

- **Guard view cones.** Enemies see in all directions today
  (`enemyCanSeePlayer` has no facing). Until idle and wandering guards have a
  forward cone, stealth works against cameras only.
- **Security console:** use a computer desk (`LEVEL_EVENT_USE_DECOR`) to shut
  the cameras down - the quiet alternative to shooting them.
- **Per-weapon noise:** a silenced weapon with a smaller `alertEnemies`
  radius, which is what makes shooting a camera quietly a real choice.
- **Alarm panels:** a guard who spots the player runs for a panel instead of
  fighting; kill them first or the alarm fires.
- **Escape timer** after an alarm, judged through `LEVEL_EVENT_EXIT`.

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

First run on device, 2026-09-21, now the baseline table in `CLAUDE.md`:
Corridor 22.2, Empty room 18.8, Detail walls 21.3, Openings 14.2, Enemies
12.4, Decorations 10.9, Crowd 6.7, average 15.2. Sprite rows are the cost:
one decoration at two cells is dearer than six enemies further away, and the
crowd's heavy filling the screen is a 149 ms frame. A second run agreed to
0.2 on one station and exactly elsewhere: under 0.3 is noise, 0.5 is real.

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

### 22. Sprite drawing cost
- [x] Partition the cost on the benchmark (2026-09-21).
- [x] Colour-run sprite format with a per-run decoder - built, measured, **rejected** (2026-09-21).
- [x] Frame cache 8 -> 9 slots (2026-09-21): the Decorations working set fits; +1 KB DGROUP, 2.5 KB to the limit. Measured: Decorations 10.9 -> 11.3, every other station on the baseline (22.3 / 18.8 / 21.3 / 14.2 / 12.4 / 6.7, average 15.2 -> 15.3).
- [x] Branch-free row decode and tighter blits (2026-09-22) - built, pixel-identical, measured: a win on large sprites only, loss below 64 px.
- [x] Use the new decoder for magnified sprites only (height >= 64), the old one below (2026-09-22). The small-sprite decoder disassembles to the same instructions as the original; +240 B DGROUP for `spriteColShift`. The fit projected Enemies / Decorations back to 12.4 / 11.3 and Crowd ~7.3-7.5; measured on device: Corridor 22.3, Empty room 18.7, Detail walls 21.2, Openings 14.2, Enemies 12.5, Decorations 11.5, Crowd 7.4 (was 6.7, 149 -> 135 ms), average 15.3 -> 15.4. Everything but Crowd is within noise of the nine-slot build.
- [ ] Add a benchmark station with an enemy about one cell away (height ~120-160): the case this was for, which only Crowd's single heavy covers now.
- [x] Clip projected sprites against the walls per column (2026-09-22). The centre column test in `draw()` still decides whether a sprite is drawn at all (and `resolvePlayerShot` still uses it, so what can be shot is unchanged); `drawProjectedSprite` now also takes `f_wallDepth`, narrows the span to the outermost columns the sprite is in front of, and masks off any hidden column inside it (`spriteColVisible`, applied per decoded row). Sprites no longer paint over a nearer wall edge - window reveals, pillars, door frames - and the hidden part is neither decoded nor blitted. Checked by the harness: with no wall in front the output matches the previous code over 6.28 million draws, and with random wall depths every visible column matches it and every hidden one keeps the background. Two golden views pin it, `map1_clip_window` and `map1_clip_pillar`; none of the benchmark stations has a sprite at a wall edge, so no station clips anything. Code +296 B, DGROUP +32 B. Measured on device against the height split: Corridor 22.4, Empty room 18.8, Detail walls 21.2, Openings 14.2, Enemies 12.3, Decorations 11.3, Crowd 7.2, average 15.3 (was 15.4). Each sprite station is 0.2 down - under the 0.3 noise floor alone, but all three and no other, so read it as a real 1-4 ms (Crowd 135 -> 139 ms). That is more than the column loops should cost (a few hundred instructions a sprite), so a register spill in `drawProjectedSprite`'s row loop from the extra locals is the likelier cause; accepted as is.
- [ ] The occlusion tests compare a sprite's perpendicular depth with a wall's distance along its ray. Off centre the wall reads up to 15% further than it is (1/cos 30 degrees at the screen edge), so a sprite standing just behind a wall near the edge of the view can still draw in front of it. Comparing against `f_wallDepth * cos(ray angle)` would fix it, in `draw()`, `resolvePlayerShot`, `drawImpact` and the per column clip together.
- [x] Decide whether a decoded-row cache per (frame, size bucket) is worth its memory - dropped 2026-10-01: branch `sprite_optimisation` removed the decode, so there is no decoded row to cache.
- [x] Per-row spans in place of the box and bands (explored 2026-10-01; built on branch `sprite_optimisation` below, the box kept for the clip and later moved into the frame). One byte a source row, 64 a frame, giving that row's first and last opaque 4-pixel group, so a row decodes only its own span instead of its 8-row band's. Counted on the PC host with the exact spans derived from the frame data (a group is one source byte): decoded pixels Enemies 2,115 -> 1,820, Decorations 2,946 -> 2,532, Crowd 6,982 -> 6,080 (its magnified heavy 3,480 -> 3,075), 13-14% fewer, every one of them transparent; the held pistol 1,316 -> 1,080 at 1:1, and the weapon blit pays a full read-modify-write even on an all-transparent byte. Empty rows inside a non-empty band: one in all the art, so that part buys nothing. Estimate, transparent small-decoder pixels at 7-11us, magnified at 4.4us, weapon at its 3.65us a pixel, +2us a row for the per-row lookup: Enemies 16.7 -> 17.5-17.8, Decorations 14.8 -> 15.6-16.0, Crowd 8.6 -> 9.1-9.2, the wall stations +0.3-1.3 fps from the weapon alone, average 22.9 -> 23.7-23.9. Encoding: 19 of 2,238 opaque rows span all 16 groups, so a 4-bit count cannot hold them; the bands' own `(first << 4) | last` with `0xF0` for empty fits every row. Memory: per-frame tables cannot sit near for all 256 possible frames (16 KB), so they travel with the frame through the far segment and the cache (+68 B a cache slot with the 4-byte box), and `spriteFrameBounds` (3,072 B for 32 x 8 frames) goes: DGROUP -2,460 net, a win in the binding segment. The table can be built at load from the pixels, so the `.spr` files need not change.
- [x] Branch `sprite_optimisation` (2026-10-01): skip the decode - the step that showed the decode was not the cost; its C row loop is now the twin of the assembler one below. `drawProjectedSprite` is now one direct scaled draw - a Y interpolant per row, the row's span byte (built at load by `buildRowSpans`, stored after each frame in its segment and the cache, 1,088 B a frame) to skip empty rows and limit the row to its groups, an X interpolant per pixel, the pixels gathered into the destination byte's opaque / black / grey bits and written to both planes when the byte is complete. Gone: the column tables, the row mask buffers, both decoders and the row reuse for magnified sprites. Interpolants and group edges are computed as before, and every golden frame is unchanged (sprite stations, magnified heavy, mirror, tiny, the window and pillar clips). Code -623 B, DGROUP unchanged (+576 span tables in the cache, -635 scratch). `tsda` shows TopSpeed spilling the pixel loop to the stack - accumulator, x, bit, the three masks, even the shift count - ~34 instructions a pixel. Predicted from the model: Enemies ~18, Decorations ~15.7, Crowd ~6.5 (the magnified heavy loses the gather decoder and the row reuse), within ~30%. **Measured slower everywhere** (profile, 2026-10-01): Sprites Enemies 32.9 -> 39.1ms, Decorations 40.4 -> 50.2, Crowd 86.7 -> 164.9 - about 19-21.5us a drawn pixel in all three, against ~11 for the old small-sprite decoder and the 12 the model gave this loop. The model was calibrated on register-bound loops; this one makes ~11 plain stack accesses and two stack read-modify-writes a pixel, so it is memory traffic the model underprices. So the decode was not the cost - the instructions and memory touches per pixel are, and this C cannot be made to keep its state in registers. Crowd also lost the row reuse (294 rows drawn against 246 decoded). The same run is the first profile since the span work: Walls mean 12.4 -> 9.5ms (Openings 38.1 -> 28.9, Corridor 16.8 -> 11.8), Rays 13.6 -> 12.1.
- [x] The same draw with its row loop in assembler (2026-10-01): [sprasm.a](sprasm.a) `spriteDrawRows`, C twin [pc/src/sprasm_pc.c](pc/src/sprasm_pc.c). `drawProjectedSprite` keeps the per-sprite set-up (clips, wall clip, the 17 group edges) and fills one global block, `spriteRows`, that the assembler reads by absolute address. Per row: the Y interpolant's source row, its span byte, the mirror, the group edges clipped, the X interpolant's start by `mul`, the row split into first / middle / last byte. Per pixel, 13 register instructions and one memory read in 27 bytes: DH of the X interpolant is the source column, the byte and bit shift come from it, and the pixel's two bits go into AH and CH by `shr`/`rcr`, so eight pixels leave the first in bit 0, the planes' order; at the write black is `CH & ~AH`, grey `AH & ~CH`, opaque their or. The eight pixel block is unrolled and entered by computed call, so a first byte enters part way and its pixels land in the top bits, and a last byte is shifted down after - no per-pixel test. Checked: frames unchanged through the C twin; in the linked image the eight 27-byte macros sit back to back, the entry adds the block's address and every `spriteRows` / `spriteColVisible` reference is the right DGROUP offset; the routine's text interpreted against the C twin over 18,911 random sprites from the real frames (7,233 magnified, 9,572 mirrored, 4,664 occluded): 0 mismatches, SI / DI / BP kept, the frame only read; four planted bugs caught. Code +314 B, DGROUP +32 (the block). Estimate, register-bound so the model should hold this time: ~3us a pixel, ~6 a byte, ~24 a row, so Sprites Enemies ~12ms, Decorations ~13.5, Crowd ~42, putting them near 26 / 25 / 14 fps; band +-50%. **Measured** (profile, 2026-10-01): Sprites Enemies 19.5ms (main 32.9, the C draw 39.1), Decorations 22.3 (40.4, 50.2), Crowd 64.8 (86.7, 164.9) - 41%, 45% and 25% under main; frames 46.2 / 48.4 / 92.3ms, ~21.6 / 20.7 / 10.8 fps against main's 16.7 / 14.8 / 8.6, and the seven stations' frames put the average near 24.9 against 22.9. About 1.6x the estimate, just outside its band. An exact fit of the three stations to pixels, rows and sprites (so rough): 5.2us a pixel (estimate 3), 59us a row (24), 295us a sprite (~100) - for Enemies 9.4ms of pixels, 8.3 of rows and 1.8 of per-sprite set-up; for Crowd 45, 17.3 and 2.4. Rows are the next lever: the row set-up reads and writes `spriteRows` fields a dozen times and makes three calls (first byte, flush, last byte) before the middle bytes; the per-sprite cost is the C set-up, 17 `scaleBound` divides among it.
- [x] The weapon on the new path (2026-10-01): `drawSprite` is now its clips and a call to `spriteBlitRows` in [sprasm.a](sprasm.a), through the same `spriteRows` block; the old 1:1 loop and its three 256-byte mask tables are gone (DGROUP -768, to 46,880, 2,272 to the limit; code -138). Unscaled, so no interpolant: the row's span byte trims it to its opaque groups (the held pistol 1,316 -> 1,080 pixels), every clip edge is a multiple of 4 so whole groups clip exactly, and each group is one `lodsb` and four pixels of `shr al,1` / `rcr ah,1` / `shr al,1` / `rcr ch,1` into the same plane bits, two groups inlined a destination byte. A sprite on the odd nibble (x & 4) fills its first byte's top half, which is where four `rcr`s put its pixels, and a last odd group is shifted down. Checked: every frame with the weapon in it unchanged (`menu_briefing` differs only because map1.map's briefing was edited); the image holds the four 33-byte group sequences, two back to back; 4,392 simulated blits from the real frames against the C twin, 2,248 on the odd nibble, 0 mismatches, and the scaled path re-run clean; three planted bugs caught - the fourth, dropping the left clip's round-up, is equivalent on every real input, since `xStart - left` is always a multiple of 4. Estimate: Weapon 4.6-4.9ms -> ~2.5-3.5ms on every station (41 rows at the scaled path's measured ~50us a row dominate). **Measured** (profile): Gun 4.6-4.9 -> 3.7-3.8ms on every station, ~1ms, just above the band; frames Corridor 30.6, Empty room 24.8, Detail walls 30.5, Openings 53.1, Enemies 45.1, Decorations 47.5, Crowd 91.9ms, so the benchmark average near 25.5 against main's 22.9. Benchmark then: Corridor 32.9, Empty room 40.6, Detail walls 33.0, Openings 18.9, Enemies 22.2, Decorations 21.2, Crowd 10.9, average 25.6 - within 0.3 of the profile's prediction everywhere; against main 31.6 / 38.6 / 31.8 / 18.6 / 16.7 / 14.8 / 8.6 (22.9), the sprite stations +5.5, +6.4 and +2.3 fps.
- [x] Lean row set-up in both sprite routines (2026-10-01). Scaled (`spriteDrawRows`): the X interpolant's start is one `imul` and `add` from per-sprite `accBase` (0 or 16383) and `advance` (+-step), so the mirror costs no branch there; a mirrored span is `xchg` and one `xor ax,0F0FH`; the slot entries are an 8-bit `mul` by 27 rather than calls to a helper; a one-byte row's shift and slot fold to 7 - q and p + 7 - q (q = (x1 - 1) & 7, p = x0 & 7); shifts by `cl`, and the stored row width is gone. Unscaled (`spriteBlitRows`): the clip groups and left / 4 are per-sprite constants (`firstGroup`, `lastGroup`, `leftGroup`); the span byte is walked by BP, the source and destination rows held in SI / DI and stepped by 16 and 32; the destination byte is the screen group leftGroup + first shifted down, its low bit the half. Code -57 B, DGROUP +16 (the five new fields, padded). Frames unchanged against the goldens once map1.map is HEAD's (the menu and objective frames that fail are map1.map's edited text: rendered with HEAD's map1 they match). Image: the new encodings present, the eight macros back to back and all three entry calculations adding the block's address. Simulated against the C twins, which read the same new fields: 11,345 scaled sprites and 4,392 blits, 0 mismatches; five of six planted bugs caught, the sixth (`imul` -> `mul`) equivalent, since the low word of a signed and an unsigned product is the same. Estimate: scaled rows ~59 -> ~45us (Enemies ~-2ms, Decorations ~-1.6, Crowd ~-4), blit rows ~45 -> ~18us (~-1.1ms every station). **Measured** (profile): Sprites Enemies 19.6 -> 18.4ms, Decorations 22.4 -> 21.5, Crowd 65.4 -> 62.7 - 8.5, 7.7 and 9.2us saved a row, against ~14 estimated; Gun 3.7-3.8 -> 3.0-3.4ms, ~10us a row against ~27. Frames Corridor 30.3, Empty room 24.2, Detail walls 30.2, Openings 52.9, Enemies 43.5, Decorations 46.2, Crowd 88.7ms: the benchmark average near 26.0. The estimate ran ~1.6x optimistic again, as it did for this routine's first version: row code is dense in taken branches, calls and memory operands, and on the V30 each taken branch or call discards the prefetch queue, so price those nearer 30 clocks than 9. The model still holds for the branch-free pixel loops.
- [x] Middle bytes inlined and the group edges in assembler (2026-10-01). A scaled row's middle bytes - always eight pixels - run their own copy of the pixel block and of the write, the write's returns turned into jumps, so a middle byte makes no call (it made two, four taken transfers); first and last bytes keep the computed entries. `groupX` is now built by `spriteDrawRows` itself from one `div`: AX carries left plus the quotient and DX the remainder, a remainder reaching the step carries into the quotient branch-free (`sub` / `sbb bp,bp` / `and` / `add` restores it, `cmp bp,1` / `adc` counts it) and a non-zero remainder rounds up (`neg` / `adc`) - against the C, where each of the 17 entries was ~17 instructions plus a call to `scaleBound` and its divide, ~14us each, ~245us of the ~295us a sprite. The carried quotient was checked against `scaleBound` exhaustively, every step 1-16384 and group 1-16, 262,144 edges, 0 different; the C twin still uses `scaleBound`, so the simulation compares the two methods directly. Code +318 B, DGROUP unchanged. Frames: the same 14 failures with the same counts as before (the 9 stale views and the 5 that show map1.map's edited text). Image: the prologue's encodings present, two runs of eight 27-byte macros - the inlined one followed by its write, the shared one by `ret` - and every entry calculation adding the shared block's address. Simulated: 11,345 scaled sprites and 2,948 blits, 0 mismatches; four planted bugs caught, among them a swap in the inlined write alone. Estimate, with branches at ~30 clocks: ~4.8us a middle byte, ~140us a sprite, so Sprites Enemies ~-1.3ms, Decorations ~-1.8, Crowd ~-4.8. **Measured** (profile): Sprites Enemies 18.4 -> 17.8ms, Decorations 21.5 -> 20.3, Crowd 62.7 -> 60.4 (main 32.9 / 40.4 / 86.7); frames 30.5 / 24.3 / 30.2 / 52.9 / 42.9 / 45.1 / 86.6ms, the benchmark average near 26.1. Half to two thirds of the estimate even with branches priced at 30 clocks; Enemies' 0.6ms is at that station's noise floor, Crowd's 2.3 is real. Where a small sprite's time goes now: ~50us a row against ~5us a pixel, so for the 11-30 pixel sprites the benchmark shows, the row overhead is about half the sprite. By instruction count the 3.7ms is ~1.7 in the ~135 destination bytes (~60 instructions each, 34 of them the eight pixels' shift and rotate pairs) and ~1.8 in the 41 rows (~45us each: the clip groups are recomputed from memory every row though they are constant for the sprite, and the span byte, source row and destination row are rebuilt from the source row number though in a 1:1 draw each is the previous one plus 1, 16 or 32). Next: a lean row set-up for both routines, then a 256-byte deinterleave table (each source byte's low and high bits split into two nibbles by one `xlat`) to cut a group from 17 instructions to ~4.
- [x] Branch wrapped up (2026-10-01). Benchmark on device: Corridor 33.4, Empty room 41.2, Detail walls 33.4, Openings 19.1, Enemies 23.4, Decorations 22.2, Crowd 11.5, average 26.3 - against main's 31.6 / 38.6 / 31.8 / 18.6 / 16.7 / 14.8 / 8.6 (22.9): +15% on average, the sprite stations +40%, +50% and +34%, and the wall stations +0.5-2.6 fps from the weapon alone (4.8 -> ~3.3ms). The profile's Frame column (30.5 / 24.3 / 30.2 / 52.9 / 42.9 / 45.1 / 86.6ms) predicted every station to within 0.5ms. Sprites against main: Enemies 32.9 -> 17.8ms, Decorations 40.4 -> 20.3, Crowd 86.7 -> 60.4. Memory: code -186 (48,866), DGROUP -720 (46,896, 2,256 to the limit), far +3,200 (64 span bytes on each of the 50 frames). `sprite.c` 3,270 -> 1,759 B of code: both decoders, their column and mask tables, the row reuse and the old 1:1 loop are gone, for ~1,318 B of `sprasm.a`. [SPRITES.md](SPRITES.md) rewritten for the new design; `memcheck` counts 1,088 B a frame.
- [x] Drop `spriteFrameBounds` (2026-10-01). The box (4 bytes) now travels in the frame, in a paragraph of its own after the span bytes (`SPRITE_FRAME_BOX`; frame 1,088 -> 1,104 B, 69 paragraphs), read into the segment from the header at load and copied out of the cached frame by `getSpriteFrame`; the band bytes are no longer kept or mirrored. Deriving the box from the span bytes instead was rejected unbuilt: a 64-iteration C scan at ~8us an iteration is ~0.5ms a cache miss, more than the copy. DGROUP 46,896 -> 43,984 (-2,912: -3,072 for the table, +144 cache, +16 load buffer), 5,168 to the limit; code -158; far +800. Near sprite data no longer scales with the slot count. Frames: the same 14 failures with the same pixel counts as before (the 9 stale views and the 5 showing map1.map's edited text), so what is drawn is unchanged. Profiling build DGROUP 44,304.

**Theoretical floor, 2026-10-01** - what sprites would cost if the only work
were stepping interpolants, reading pixels and writing them. Units counted on
the PC host per frame (sprites / rows / pixels drawn / pixels decoded / full,
partial and skipped destination bytes): Enemies 6 / 142 / 2,115 / 2,115 / 54,
261, 91; Decorations 8 / 117 / 2,946 / 2,946 / 149, 209, 85; Crowd 8 / 294 /
9,967 / 6,982 / 569, 619, 347 (48 magnified rows reuse their decode); the
weapon 41 rows, 1,316 pixels at 1:1. Priced with a model calibrated on this
session's measured loops (~2.4 clocks a byte of code fetched, ~21 a
read-modify-write, ~10 a plain memory access, ~9 a taken branch: it gives the
C fill loop's 54 clocks a row and the unrolled span's 31) for an ideal kernel:
the column table built once a sprite by the x interpolant, a pre-expanded
source of one byte a pixel holding its opaque / black / grey bits, eight
pixels shifted into the byte being built without a branch, then both planes
stored (whole byte) or and-ed / or-ed (partial). That is ~63 clocks (2.3us) a
pixel, 1.5us a whole byte, 4.7us a partial byte, 4us a row, 16us a sprite.
Floor: Enemies 7.1ms against 32.9 measured, Decorations 8.9 against 40.4,
Crowd 21.9 against 86.7 - 4 to 4.7x - and the weapon, stored pre-built as
planes since it never scales, 1.1ms against 4.8. The bus alone (every byte
moved, no instructions) is 0.2-0.6ms, so the floor is instruction-bound,
not memory-bound. Today's code spends 8.7-15.6us a drawn pixel against the
floor's 2.3; the per-row and per-source-byte overheads of the small-sprite
path are most of the gap (37us a row and 10us a source byte, measured
2026-09-22). With sprites and weapon at the floor and nothing else changed:
Corridor 35.9, Empty room 44.6, Detail walls 35.9, Openings 20.0, Enemies
32.9, Decorations 30.9, Crowd 21.0 fps, average 31.6 against 22.9 now; with
the kernel 30% cheaper or 40% dearer, 29.9-33.2. The floor assumes formats
with a memory cost: a pre-expanded source is 4 KB a frame (the near cache
holds 9 KB), and a pre-built weapon 1.5 KB a frame.

The row decode (2026-09-22). The TopSpeed disassembler (`tsda SPRITE out.txt`
in DOSBox) showed the per-pixel loop in `buildSpriteRowMasks` was ~22
instructions with 7 memory reads and 4 branches: its three mask
accumulators, loop limit and row pointer lived on the stack, and each pixel
took a variable `shr ax,cl` and a branch per colour. The rewrite unpacks the
source columns a row samples to a byte each (`spriteRowPix`, once per
distinct source row), gathers eight destination pixels into a word in packed
2bpp order, and converts them with the three 4-pixel mask tables as the 1:1
weapon path does: 8 instructions a pixel, 2 memory reads, no branches, plus
~20 a destination byte. The unpack is extra work per source row, so the gain
grows with sprite width and is estimated about even at 20-25 px - the
benchmark's 11-30 px sprites sit near that, and a station with a close
sprite would show the case this is for. Both blit loops now keep one plane
pointer (the grey plane is `BM_BYTES` after the black) instead of reloading
`blackBm` and `greyBm` from memory every byte, and store fully opaque bytes
without reading them. Checked against the previous code by a PC harness
drawing both over random backgrounds - every `spr/` frame and 12 random
ones, heights 1-960, every span, three offsets, plain and mirrored, and the
weapon at every x and every third row: 6.28 million draws, no difference.
DGROUP -176 bytes.

Measured on device against the nine-slot cache build: Corridor 22.3 -> 22.3,
Empty room 18.8 -> 18.6, Detail walls 21.3 -> 21.1, Openings 14.2 -> 14.2,
Enemies 12.4 -> 11.7, Decorations 11.3 -> 11.1, Crowd 6.7 -> 7.1, average
15.3 -> 15.1. The blit changes bought nothing measurable (Corridor draws
only the weapon). The PC host counted, per station frame (Enemies /
Decorations / Crowd): 142 / 117 / 246 decoded rows, 406 / 443 / 1103
gathered bytes, 921 / 1290 / 1856 unpacked source bytes. Fitting the new
decode time (task 22's ablation plus the measured delta) to those gives
about **35 us per gathered byte (4.4 us a pixel against the old 11), 10 us
per unpacked source byte, and 37 us per row** of fixed overhead - three
stations, three unknowns, so a rough fit with no check on it. Per sprite
that puts break-even near height 64: the 60 px sprites come out about even,
the smaller ones lose (a 15 px row unpacks and gathers two to three times
the pixels it draws, and pays the 37 us), and Crowd's one h=120 heavy
gains about 13 ms, which is all of that station's improvement.

The benchmark said sprite rows were the cost; temporary ablation switches
in [sprite.c](sprite.c) (`SPRITE_ABL_NO_DECODE`, `_NO_BLIT`, `_NO_WEAPON`,
each removing work rather than substituting a value) said which part. They
were taken out once the figures below were measured; to partition it again,
put them back for the run - the decode one holds the first row's masks for
the whole sprite, the blit one decodes and writes nothing, the weapon one
returns from `drawSprite` at once. Per frame removed on Enemies /
Decorations / Crowd: the per-pixel row decode 24.4 / 31.8 / 75.8 ms, the
per-byte blit 4.3 / 4.7 / 14.2, the weapon overlay 5.2 flat on every
station, and on Decorations a further 3.2 from nine distinct frames (four
decorations, four pickups, the weapon) cycling through the eight-slot LRU
cache, which misses on every access when the working set is one larger - a
ten-slot build confirmed it alone. The PC host counted 2115 / 2946 / 6982
pixel decodes and 406 / 443 / 1535 blitted bytes for those frames, so the
decode costs about 11 us - 300 clocks - per destination pixel and the blit
9 us per byte, both linear. Half the Crowd frame was the decode.

The experiment: a format 2 storing each frame as colour runs per row (2-bit
colour, 6-bit length, a 64-entry row offset table, 274-847 bytes a frame
against 1024) and a decoder that maps a run to a screen span through a
per-sprite source-column-to-screen-column table and ORs it into the row
masks - Doom's column-and-post idea turned through ninety degrees for a
row-major screen, with the post being a colour rather than pixels. It was
pixel-identical (a simulation over every width 1..640 matched the old
sampling for all 205,120 columns, plain and mirrored; every golden frame
matched) and saved 2.6 KB of DGROUP, and the weapon went through it too.

Measured, it lost, and equally with the decoder in C and in assembler
(`sprasm.a`, ~70 instructions a run including the span routine, the same
numbers to 0.1 fps): Corridor 22.2 -> 17.7, Empty room 18.8 -> 15.4,
Detail walls 21.3 -> 17.1, Openings 14.2 -> 12.2, Enemies 12.4 -> 9.6,
Decorations 10.9 -> 9.4, Crowd 6.7 -> 6.3. Corridor has no sprite but the
weapon, so its +11.5 ms is the weapon alone: the old 1:1 path decodes four
pixels per table lookup (~135 instructions a row) and the run path walks
~7 runs a row (~490). Taking that off the rest leaves the run decoder at
+13.5 / +4.5 / +0.5 ms on Enemies / Decorations / Crowd. The unit count did
fall - runs on exactly what the stations draw were 1328 / 1304 / 2496
against those pixel counts, 1.6x / 2.3x / 2.8x fewer - but at on-screen
sizes (11-30 px) a run covers 1-4 destination pixels and costs 2-3.5 of
them, so it is a wash at best and a loss with the weapon. Only the h=120
heavy gained (5x fewer units), and one sprite is not a frame.

Two lessons for the next attempt, both now in `CLAUDE.md`. Fewer units is
not the lever at these sizes; the per-unit cost is, and it is set by the
instruction count of the loop, not by who writes it: TopSpeed's code for
these loops is as tight as hand assembler (the C and assembler decoders
measured identically), so "rewrite in assembler" is only a lever where C
cannot reach an instruction, as with `fpmul` and `IMUL`. And a 1:1 special
case is worth keeping: the weapon's four-pixels-per-lookup path is a third
of the cost of the general one.

Reverted the same day: format 1 files, the pixel decoder and `drawSprite`
are back, `sprasm.a` / `sprasm.h` / `pc/src/sprasm_pc.c` / `spr/README.md`
are gone, and the ablation switches came out with them. The row in `CLAUDE.md`'s rejected table points
here. What is left on the table: the cache slots (a define, measured), and
the only remaining large lever - not decoding at all on most frames, by
caching decoded mask rows per (frame, size bucket, bit phase) so a standing
or slowly approaching enemy costs only its blit. That is a redesign with a
memory problem (a decoded 60 px frame is ~1.8 KB, DGROUP has ~3.5 KB free,
far memory has 400 KB but costs 0.35 ms per KB to bring near) and wants
its own costing before anything is built.

### 26. Subsystem profile
- [x] A profiling build of the benchmark that splits each station's frame into its parts (2026-09-30).
- [x] Run it on the device and replace the 2026-09-04 budget table in `CLAUDE.md` with its numbers (2026-09-30).
- [x] Count units per station on the PC host and fit the profile to them (2026-09-30, temporary counters, not kept).
- [x] Find out why Decorations is 80.2ms in the profiling build and 88.5ms in the normal one: it was not; the photo's 88.2 was misread as 80.2 (see below).
- [x] Profile again after the DDA work (2026-09-30).
- [x] `bmFillPattern4` in assembler (2026-10-01), the dither span (Openings 100 calls / 599 rows, Corridor 49 / 200, Detail walls 18 / 104). Its C row loop was twelve instructions with the row counter and the mask kept on the stack, three memory accesses a row besides the pixel byte. Now in [bmasm.a](bmasm.a) with the fill's shape: the C clipping in registers, then a jump into 160 unrolled rows of `mov al,[bx][d]` / `and al,ch` / `or al,dl` (or `dh`) / `mov [bx][d],al`, 12 bytes an entry, the keep mask in CH and the two alternating pattern bytes fixed to even rows (DL, the first row's) and odd rows (DH), so nothing toggles at run time. The table's fixup (`add si,B1F3H`) and its 12-byte entries (1,908 bytes first to last) checked in the linked image. The simulator now runs all three spans, 3.73M cases, 0 mismatches; four planted pattern bugs caught, one a single row given the wrong pattern register. (The simulator itself first reported 279k false mismatches: it modelled every table as 4 bytes an instruction, and now scales each table by its real entry size and fails on a jump into the middle of an entry.) Code +1,897 B (`_TEXT` 49,052 of 64K), DGROUP unchanged, frames unchanged. Estimate: a row ~5-6us in C to ~1.5-2us, but the entry is ~9 instructions longer than C's, so Openings about -2ms, Corridor about -0.5ms, the rest within noise. Measured on device: Corridor 31.0 -> 31.6, Empty room 38.8 -> 38.6, Detail walls 31.4 -> 31.8, Openings 17.9 -> 18.6 (2.1ms, estimate ~2), Enemies 17.0 -> 16.7, Decorations 15.1 -> 14.8, Crowd 8.8 -> 8.6, average 22.8 -> 22.9. The three sprite stations read 1.1-2.6ms slower, though they draw 0-3 dither rows and no sprite code changed. Not layout: everything after `bitmap.c` moved 140 bytes, even, so every loop keeps its word alignment. Read against the build before the span work, the two runs together put Enemies / Decorations / Crowd at 16.7 / 14.8 / 8.6 where the counted rows predict ~16.8 / 14.95 / 8.75: the span run read high on those stations (it gained twice what their rows explained), and this one is back on the line. So the 0.3 fps noise floor, set on the ~22 fps stations, is 1-1.3ms on a 15-17 fps one, and a lone sprite-station reading should be repeated before it is acted on.
- [x] Wall span rows (2026-10-01). Counted on the PC host per station (calls / rows): `bmFillRect4` carries it - Openings 624 / 4158, Corridor 230 / 3977 - then `bmClearRect4` (Openings 127 / 2710), `bmFillPattern4` small (Openings 100 / 599), `bmFillCol1` unused on the benchmark. Of the 24us a span call, the C primitive's entry is ~28 instructions (~15us); the rest is the wall style deciding the span. Its row loop is five instructions (`or`, `add`, `dec`, a redundant `test`, `jne`), the measured 2us a row. `bmFillRect4` and `bmClearRect4` are now [bmasm.a](bmasm.a): the same clipping in registers, then a jump into a 160-entry unrolled run of `or [bx][disp],dl` (`and` for the clear), one instruction a row, entered so exactly `h` rows run. The row pointer is biased by 128 so every entry takes the 16-bit displacement form and is 4 bytes; the table offsets were checked in the linked image (`add ax,ACB5H` is where the fill table lands, 636 bytes first to last entry). Checked by interpreting `bmasm.a`'s text against the C (now [pc/src/bmasm_pc.c](pc/src/bmasm_pc.c)) over 2.49M x / y / h cases through every clipping edge, 1.42M of them drawing: 0 mismatches, nothing written outside the buffer, SI and DI kept; four planted bugs caught, one of them a single table entry's displacement. One difference by construction: C's `y + h > 160` overflows s16 for y above ~2000 with a huge h, where it would have written past the buffer; the assembler compares `h` with `160 - y` and draws nothing. Code +1,286 B (47,155 of 64K), DGROUP unchanged, frames unchanged. Estimate, rows 2us -> ~0.7-0.9us: Openings -7.5 to -9ms, Corridor -4.5 to -5ms, Detail walls ~-2.5ms, sprite stations under 1ms. Measured on device: Corridor 28.0 -> 31.0, Empty room 38.2 -> 38.8, Detail walls 29.8 -> 31.4, Openings 16.2 -> 17.9, Enemies 16.6 -> 17.0, Decorations 14.8 -> 15.1, Crowd 8.7 -> 8.8, average 21.7 -> 22.8. Over the counted fill and clear rows the three wall stations agree on 0.85-0.87us saved a row (11.0ms over 12,882 rows), so an unrolled row costs ~1.15us (~31 clocks for the read-modify-write), not the 0.7-0.9 estimated: the saving came in ~25% under. The sprite stations read more than their rows explain, inside their noise. Left in C: `bmFillPattern4`, and the wall styles' own per-span work.
- [x] 186 instruction set (2026-10-01). `#pragma optimize(cpu=>186)` is refused ("Illegal pragma value for cpu"); TopSpeed's targets go 86 -> 286, so `unnamed.pr` now has `cpu=>286` after `#model`, overriding the SDK's EPOC `cpu=>86`. Safe on the V30 as TopSpeed uses it: every C module's `tsda` listing (all 23) has no `0F`-prefixed opcode, no `arpl` and no 286 system instruction, the only things in the 286 set the V30 lacks. But it emits just one new form: `push` of an immediate, 58 sites (48 short), `mov ax,n; push ax` becoming `push n`. No shift by an immediate (it still loads `cl`, 163 sites), no `enter`/`leave`, no `imul` by an immediate. Code -228 B, DGROUP unchanged, frames unchanged (the PC build does not see it). The pushes are in the menus (25) and HUD (11); the render has four on the pillar `boxHit` call in `draw()` and a handful in the wall styles and `bitmap.c`, so no measurable frame change was expected. Measured on device: Corridor 27.4 -> 28.0, Empty room 38.2 -> 38.2, Detail walls 29.6 -> 29.8, Openings 16.0 -> 16.2, Enemies 16.4 -> 16.6, Decorations 14.6 -> 14.8, Crowd 8.6 -> 8.7, average 21.5 -> 21.7: 0.2-1.3ms faster, six stations up and none down, only Corridor clearly past the noise on its own. More than the push-immediates in the render look worth; not isolated (every function after the first changed module also moved). Kept: no slower, and 228 B smaller.
- [x] Per-ray set-up (2026-10-01). The fitted 4.9ms "fixed" part of Rays is ~1ms of once-a-frame drawing inside `draw()` (the `bmDrawRect` border's 158-row edge loop ~0.7ms, the crosshair's four `bmXorLine`s ~0.3ms, by instruction count) and ~65us a ray. Of the ~80 instructions a ray spent before its first walk, the loop-invariant ones are now worked out once a frame above the loop: the player's cell (two `cbw` pairs a ray), the start cell pointer (`&map[y][x]`, a shift by 7), and the four distances from the player to the cell's edges. Each sign branch now loads its distance, makes its one `fpmul`, stores the side distance straight into the ray block (no `sidedx`/`sidedy` locals copied across) and sets its quadrant bit, so the second compare chain `DDA_QUADRANT` compiled to is gone (the macro with it). ~50 instructions a ray plus the two `fpmul`s: ~30 fewer, ~16us at ~15 clocks each, ~1ms a frame on every station whatever it shows. Frames unchanged, code -6 B. Measured on device: Corridor 26.8 -> 27.4, Empty room 37.2 -> 38.2, Detail walls 29.0 -> 29.6, Openings 15.7 -> 16.0, Enemies, Decorations and Crowd unchanged at 16.4 / 14.6 / 8.6, average 21.2 -> 21.5. The three stations precise enough to show it saved 0.7-0.8ms, ~12.5us a ray against ~16 estimated; on the sprite stations 0.75ms is 0.1-0.2 fps, inside their noise. Left: the border (a pointer walk with a per-row `if(twoEdges)`) and the crosshair are the other ~1ms of the fixed part.
- [x] Wall hit maths (2026-10-01), the 52us a hit left in the ray cast. `tsda DRAW` showed ~90 instructions a hit around four calls, three multiplies and a divide. Removed, results bit-identical: the `mapCellType` call for the pillar test (now a masked compare with the pre-shifted type) and the `isSolid` call (one `test ah,40H`); the multiply by 13 at every `wallhits[hits]` index, both when the hit is stored and in the draw loop (a `wallhit_t*` walked along the array, `add si,0EH` a hit); `f_wallx - int2fp(fp2int(f_wallx))` as `f_wallx & 0xff`, which it is exactly; and `wallhit_t` padded 13 -> 14 bytes so no hit's words sit on odd addresses. The two `fpmul`s, the height `idiv` and the footprint `mul` are the real work and stay. ~27 instructions a hit, so an estimated ~15us of the 52 at ~15 clocks each: ~1ms a frame, ~2ms on Openings (142 hits). Frames unchanged, code -24 B. The compiler hoisted the pillar mask to just after the walk, two instructions a stop. Measured on device: Corridor 26.2 -> 26.8, Empty room 36.0 -> 37.2, Detail walls 28.4 -> 29.0, Openings 15.5 -> 15.7, Enemies 16.1 -> 16.4, Decorations 14.4 -> 14.6, Crowd 8.5 -> 8.6, average 20.7 -> 21.2. Pooled, 6.8ms over 532 hits is 12.7us saved a hit (estimate ~15), so a hit's maths is now ~39us; Openings, the most hits, gained least (0.8ms against ~2 estimated), inside its 1.2ms noise floor.
- [x] Screen clear in assembler (2026-09-30): `bmClearScreen` is [bmasm.a](bmasm.a), one `rep stosw` of 5120 words over both planes, the 16 hidden columns included (skipping them would need a loop per row for 1/16 of the words); the unrolled C loop it replaces measured 4.1ms. The C twin is [pc/src/bmasm_pc.c](pc/src/bmasm_pc.c) (`memset`). The buffer's address comes from `extrn _screenBm`: checked in the linked image, the routine's `mov di` loads 222EH, which is `_screenBm` in PSION3D.MAP. Frames unchanged, code -79 B. Profiled on device: Clear 4.1 -> 1.1ms (1.0-1.4 by station), 3.0ms off every frame, mean frame 61.9 -> 59.1ms; per station Frame 38.2 / 27.9 / 35.5 / 65.2 / 61.9 / 69.0 / 116.6ms, which the profile's Frame has matched in the normal build to 0.6ms, so about 26.2 / 35.8 / 28.2 / 15.3 / 16.2 / 14.5 / 8.6 fps. The normal benchmark then read Corridor 26.2, Empty room 36.0, Detail walls 28.4, Openings 15.5, Enemies 16.1, Decorations 14.4, Crowd 8.5, average 19.5 -> 20.7: within 0.2 of the prediction at every station. That is ~0.21us (~6 clocks) a word, above the 0.4-2.5ms estimate, which scaled from the blit: the blit's `rep movsw` costs 0.71us a word, so its 3.4ms is video RAM and not code. Walls read 0.3-0.5ms higher at every station, at the noise floor of a two-pass difference and not in proportion to rows drawn, so not taken as real.
- [x] DDA step accessors written out by hand in `draw()` (2026-09-30): `mapCell`, `isWall`, `isSolid`, `isSprite`, `isMarked`, four near calls a step through open floor, now a bounds check, one `map` load and `test ah,imm` tests. Every golden frame unchanged, `_TEXT` and DGROUP the same size, and every function after `draw()` at the same address, so the sprite and wall code cannot have moved. Measured on device: Corridor 22.3 -> 23.6, Empty room 18.7 -> 23.0, Detail walls 21.2 -> 23.6, Openings 14.2 -> 14.8, Enemies 12.3 -> 13.5, Decorations 11.3 -> 12.2, Crowd 7.2 -> 7.6, average 15.3 -> 16.9. The saving per DDA step is 7.6-9.6us on every station (8.6 on average, a third of the 25.6), so the gain is the step loop alone and a step now costs ~17us. Still on the stack each step: `sidedx`, `sidedy`, `mapx`, `mapy`, `hitcell`, `hit`.
- [x] DDA step in registers, indexed by pointer (2026-09-30): `ddaWalk` in [ddaasm.a](ddaasm.a), C twin [pc/src/ddaasm_pc.c](pc/src/ddaasm_pc.c). The ray keeps a pointer to its `map[][]` entry and steps it by +-1 / +-64 cells; `mapx`/`mapy` are rebuilt from the pointer only at stops. The walk stops where `draw()` has work - a wall, or a sprite not yet marked - and `draw()` applies the old tests there, so it passes nothing the old loop acted on. One copy of the loop per quadrant so both steps are immediates; all six values in registers (`si` cell, `ax`/`cx` side distances, `dx`/`di` deltas, `bx` the cell), one memory read a step. No bounds check: `loadMapFile` now fails a map whose edge is not all solid (every map passes; `map/README.md` has the rule), and nothing at run time opens a solid cell. Why assembler: the same loop as its own C function got four of the six into registers and never used `di`, and `register` changed nothing - register allocation is what C cannot reach. Checked: every golden frame unchanged (through the C twin); `ddaasm.a`'s own text interpreted against the C twin over 200,000 random walks on random solid-bordered maps, 0 mismatches, all quadrants and both faces exercised, and three planted bugs each caught; the assembled bytes hold every key instruction the expected number of times (`tsda` hangs on assembler objects). Code -60 B, DGROUP unchanged. Measured on device against the inlined C step: Corridor 23.6 -> 24.6, Empty room 23.0 -> 32.2, Detail walls 23.6 -> 26.2, Openings 14.8 -> 14.6, Enemies 13.5 -> 15.2, Decorations 12.2 -> 13.7, Crowd 7.6 -> 8.3, average 16.9 -> 19.2 (15.3 before task 26's two DDA changes). Fitted to counted steps and stops (a stop being a wall hit or collected sprite, where the walk returns to `draw()`), every station within 0.7 ms: **13.4us saved a step, so a step is now ~3.6us** (from 25.6 at the start), and **34us added a stop**. Openings has twice the stops of any other station (142) and few steps (299), so it lost 0.9 ms net. The stop cost is the call itself: ~32 instructions of `ddaWalk` entry and exit (six pushes, the ray loaded, the quadrant dispatch, three stores, six pops) and ~20 in `draw()` rebuilding `mapx`/`mapy` from the pointer.
- [x] Trim the per-stop cost (~34us x 63-142 stops, 2.2-4.9 ms a frame), 2026-09-30. Done: the walker saves no general register (`reg_saved=>(es,ds,st1,st2)`; the compiler saves nothing around the call because `draw()` has nothing live across it); one entry point per quadrant, `ddaWalk0`..`ddaWalk3`, picked once per ray through `ddaWalkers[]` in `draw.c`, so there is no dispatch at a stop; `mapx`/`mapy` rebuilt from a byte offset, two instructions shorter. A stop is now 13 walker instructions against ~32. The function pointer type is declared under the same `#pragma call`, and TopSpeed checks it: giving the typedef alone a different `reg_param` fails the build ("'reg_param' attributes do not match"), so a call through the table cannot use the wrong convention. Checked: frames unchanged, 200,000 simulated walks of the new text against the C twin with 0 mismatches and planted bugs caught, encodings byte-scanned. Code +18 B, DGROUP +8 B (the table). Measured on device: Corridor 24.6 -> 24.8, Empty room 32.2 -> 33.0, Detail walls 26.2 -> 26.4, Openings 14.6 -> 15.0, Enemies 15.2 -> 15.5, Decorations 13.7 -> 13.9, Crowd 8.3 -> 8.3, average 19.2 -> 19.5. Each station alone is at the noise floor, but six of seven moved up and none down; pooled, 5.5 ms over 554 stops is **10us saved a stop**, against ~11 estimated from ~20 fewer instructions at the measured ~15 clocks each, so a stop is now ~24us. Not tried: handing `mapx`/`mapy` back from the walker (it has no register spare to keep them), The other fit for Openings' dip, wall code slowed by moving, is ruled out: the second profile's Walls did not move.

First run, 2026-09-30 - the table is in `CLAUDE.md`. Mean frame 75.0ms:
rays 26.6, sprites 23.1, walls 11.9, weapon 4.7, clear 4.1, blit 3.4, other
0.3, residue 0.7 (at most 1.3 anywhere, so the switches do separate). The
profiling build's Frame matched a normal run the same day to 0.6ms on six
stations (Corridor 45.1 against 44.8, Crowd 139.5 against 138.9), so the
switches themselves cost nothing measurable.

Units counted per station on the PC host, one frame (steps, wall hits,
sprites projected, span calls, rows): Corridor 296/65/0/286/4244, Empty
room 1131/63/0/78/561, Detail walls 514/69/0/334/2074, Openings
299/142/0/851/7467, Enemies 820/65/6/93/786, Decorations 818/63/8/75/725,
Crowd 968/65/8/100/693. Least squares on those gives rays = 7.5ms +
25.6us x steps + 56us x (hits - 60) + 0.18ms x sprites, and walls =
14us x hits + 24us x spans + 2us x rows, each reproducing all seven
stations within 0.5ms.

What that points at, not yet tried:
- **The DDA step, 25.6us (~690 clocks).** `tsda DRAW` shows three near calls
  a step - `mapCell` (bounds check, push/pop, `mov cl,7; shl`), `isWall`,
  `isSolid` - because TopSpeed emits the `static` accessors in
  `game_map.h` as functions, and `sidedx`, `sidedy`, `mapx`, `mapy` all
  live on the stack. It is 7.6ms of Corridor, 29ms of Empty room and
  ~21ms of each sprite station. TopSpeed has an `inline` keyword and an
  `inline_max` option (keyword table in `TS\SYS\TSC.TXT`); walking a cell
  pointer by +-1 / +-64 instead of recomputing the index is the other half,
  but drops the bounds check, so it needs every map to have a solid border
  (task 21 could enforce it).
- **The span call, 24us** against 2us a row - the existing rule, now with a
  number. Openings pays 20ms of its 37ms walls in calls.
- **`cpu=>86`.** The SDK's EPOC system block in `TS\SYS\TSPRJ.TXT` sets
  `#pragma optimize(cpu=>86)`, and TopSpeed knows V30 / 80186 targets. The
  V30 runs the 186 set (shift by immediate, `push imm`, `imul` by
  immediate), and the listings are full of `mov cl,n; shl`. One line in
  `unnamed.pr`; needs checking that the emulator and the OS are happy with
  what it emits, then a benchmark.
- **The clear, 4.1ms**, is an unrolled C loop over all 10,240 bytes, while
  the blit moves the same amount with reads in 3.4ms. A `rep stosw` in
  assembler, and skipping the 16 hidden columns, are the obvious tries.

Decorations was first transcribed from the photo as 80.2ms, and chased for
an afternoon as an 8ms difference from the normal build (88.5), with a
code-alignment theory to explain it. It was a misread 8: Frame is the sum of
the other columns by construction, and that row sums to 88.2, which the
Mean row (75.0) also needs. There was no difference between the builds.
Check a transcribed row adds up before reading anything into it.

Second run, 2026-09-30, after the three DDA changes (table in `CLAUDE.md`):
mean frame 61.9ms - sprites 23.0, rays 13.6 (was 26.6), walls 12.0, weapon
4.8, clear 4.1, blit 3.5. Walls did not move (Openings 37.2 -> 37.7), so
Openings' dip after the assembler walk was the stop cost, not wall code
slowed by moving. Taking the benchmark-fitted step (3.6us) and stop (24us)
costs out of Rays and fitting the rest to the same counts reproduces every
station within 0.1ms: 4.9ms fixed (81us a ray), 52us of maths per wall hit,
0.14ms per sprite projected. The profile's Frame matched the normal
benchmark within 0.6ms on every station.

`.\tools\profile.bat` builds `PROF3D.IMG`: the normal game with
`BENCH_PROFILE` defined, from a copy of the sources in `profbld\` so its
OBJs never mix with the normal build's (tsc /m rebuilds by timestamp, not by
define). Options > Benchmark in that image measures every station in eight
passes of `BENCH_PASS_TICKS` (6 s) - whole frame, no clear, no blit, no
weapon, no world sprites, walls drawn twice, rays only (no walls, sprites or
weapon), no draw at all - about 6 minutes for the seven stations, and ends
on a PROFILE table in ms per frame: Frame, Rays, Walls, Sprite, Gun, Clear,
Blit, Other and Resid, a row per station and their mean. Each part is the
difference between two passes that differ in that part alone
(`benchPartMs10` in [bench.c](bench.c) has the formulas). Walls are doubled
rather than removed because a wall's return value decides which sprites it
hides; the rays are measured with everything that reads that value off.
Other is the loop around the render - `wFlush`, HUD, input, event poll -
and Resid is the frame less the sum of the other parts, which should be
noise: if it is not, two switches are not separable.

Precision: a pass is ~192 ticks timed to a tick, so a pass reads to ~0.5%
and a part, as the difference of two, to about 0.3 ms at 45 ms a frame and
1 ms on Crowd's 135 ms. The Frame column is the profiling build's own frame
time; compare it with a normal benchmark run to see what the switches
themselves cost (one test per wall hit and a few per frame).

None of it reaches `PSION3D.IMG`, which hashes as before. The hooks in
`gameloop.c` and `draw.c` are `#ifdef BENCH_PROFILE`: written first as
`if(!BENCH_SKIP(...))` over a constant 0, both files compiled to a
different image, so TopSpeed does not drop a constant-false test without a
trace. The one in `psion3d.c` did hash unchanged and keeps the macro.
Profiling build DGROUP 47,936 (+320 over the normal build), 1,216 to the
limit. On the PC, `-DPSION3D_BENCH_PROFILE=ON` in a build directory of its
own (`pc/build-profile`) runs the same passes: `--bench --frames 11000
--screenshot` reaches the table, where every part is 0 and Other is the
whole 31.3 ms, because the virtual clock makes every frame exactly one tick.

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
