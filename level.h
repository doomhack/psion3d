#ifndef LEVEL_H
#define LEVEL_H

#include "fp_types.h"

/*  Per-level scripting. The game reports what the player did to a cell and
    the level decides what it means: which door a keycard opens, which desk
    completes an objective, whether a shot camera is destroyed. One handler
    per level, selected in loadMapData() alongside the wall style, with
    levelEventNone for any level that has no script.

    A handler returns TRUE when it dealt with the event. On FALSE the game
    applies the default, which is what every level did before scripts
    existed: a keycard or switch opens every locked door on the level, and
    the other events do nothing. So a script opts out of a default by
    handling the event, and a level without a script plays as it always has.

    Events, with what item and x, y carry:

    LEVEL_EVENT_PICKUP       item = PICKUP_TYPE_*, x, y = the cell it lay in.
                             The cell is already clear when this fires, so a
                             handler that rewrites the map is not undone.
    LEVEL_EVENT_USE_DECOR    item = DECOR_TYPE_* (frame, DECOR_TYPE_BIT off),
                             x, y = the decoration's cell.
    LEVEL_EVENT_SHOOT_DECOR  item and x, y as above. The round stops at the
                             decoration; the impact marker is already placed.
    LEVEL_EVENT_USE_SWITCH   item = 0; the cell is the switch. The thrown flag
                             is already written to the cell.
    LEVEL_EVENT_KILL         item = ENEMY_TYPE_*, x, y = the cell the enemy
                             died in. Only the player kills, so this is
                             always a player kill. The cell still carries the
                             enemy until the corpse releases it, so a script
                             after a particular enemy can recover it with
                             getEnemy(GET_CELL_ID(mapCell(x, y))).
    LEVEL_EVENT_EXIT         item = 0, x, y = the level's end cell, on the tick
                             the player enters it (once per entry, not every
                             tick standing there). This is where an objective
                             that is "still true at the end" - nobody harmed,
                             nothing tripped - is completed. It does not end
                             the level; that is TASKS.md task 10.

    What a handler has to work with, all of it existing code:
    updateCell(x, y, MAP_MASK_WALK) removes a sprite cell (a decoration that
    is destroyed, or a pickup); updateCell(x, y, makeDecorCell(t)) swaps a
    decoration for another frame (a wrecked version of itself);
    unlockDoor(x, y) opens one door and unlockDoors() all of them;
    missionSetObjective(i, OBJECTIVE_COMPLETE / OBJECTIVE_FAILED) moves the
    objective the pause screen shows and tells the player so. */

#define LEVEL_EVENT_PICKUP 0
#define LEVEL_EVENT_USE_DECOR 1
#define LEVEL_EVENT_SHOOT_DECOR 2
#define LEVEL_EVENT_USE_SWITCH 3
#define LEVEL_EVENT_KILL 4
#define LEVEL_EVENT_EXIT 5

typedef u16 (*level_event_fn)(const u8 event, const u8 item, const u8 x, const u8 y);

/* Never NULL: loadMapData sets it and it starts as levelEventNone. */
extern level_event_fn levelEvent;

u16 levelEventNone(const u8 event, const u8 item, const u8 x, const u8 y);
u16 levelEventMap1(const u8 event, const u8 item, const u8 x, const u8 y);

#endif
