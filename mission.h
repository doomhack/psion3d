#ifndef MISSION_H
#define MISSION_H

#include "fp_types.h"
#include "game_map.h"

/*  The mission index, the difficulty setting, and the per-mission progress
    the menus show. The index is built once at startup by parsing the level
    files, so the mission list is the map directory and nothing else. */

/*  map1.map .. map20.map are the missions, in order; the scan stops at the
    first one missing, which is what keeps the showcase maps 98 and 99 out. */
#define MISSION_MAX 20
#define MISSION_NAME_LEN 32

#define DIFFICULTY_AGENT 0
#define DIFFICULTY_SENIOR 1
#define DIFFICULTY_ELITE 2
#define DIFFICULTY_COUNT 3

#define OBJECTIVE_INCOMPLETE 0
#define OBJECTIVE_COMPLETE 1
#define OBJECTIVE_FAILED 2

/*  How the mission in play ended. KIA is health at zero. The other two are
    judged on the end cell: COMPLETE when every objective is complete,
    FAILED when any is failed or still open. */
#define OUTCOME_NONE 0
#define OUTCOME_COMPLETE 1
#define OUTCOME_FAILED 2
#define OUTCOME_KIA 3

/*  missionIndex when the level in play is not in the index (map 98, 99). */
#define MISSION_NONE 0xff

/*  A best time that has never been set. */
#define MISSION_TIME_NONE 0xffff

extern u8 missionCount;
extern u8 difficulty;

/*  The mission in play: its index (or MISSION_NONE), ticks since it began,
    and how it ended. */
extern u8 missionIndex;
extern u16 missionTicks;
extern u8 missionOutcome;
extern u8 missionNewBest;	/* the last outcome set a best time */

/*  One state per mapInfo objective, OBJECTIVE_*. Reset at mission start and
    moved by the level script (level.c) as the player acts; read by the
    objectives screens and judged by missionTick at the exit. */
extern u8 objectiveState[MAP_MAX_OBJECTIVES];

/*  Move objective i (0-based) to OBJECTIVE_COMPLETE or OBJECTIVE_FAILED and
    tell the player with an info message, "Objective 2 Completed". A change
    to the state it already has is silent, so a script can set it freely.
    Level scripts go through this rather than writing objectiveState[]. */
void missionSetObjective(const u8 i, const u8 state);

void missionScan(void);

/*  A mission has begun: mapId's index (if it has one), the clock and the
    objectives all reset. */
void missionStart(const u8 mapId);

/*  One tick of the mission in play. Counts the time and tests for the end
    cell and for death, returning OUTCOME_NONE while play goes on and the
    outcome once it does not; on an outcome the best time is updated. */
u8 missionTick(void);

/*  The best time (ticks) for a mission at a difficulty this session, or
    MISSION_TIME_NONE. Best times live in the index segment, so they cost no
    near data - and no persistence yet: they go with the process. */
u16 missionBest(const u8 mission, const u8 d);

static u8 missionMapId(const u8 mission)
{
	return (u8)(mission + 1);
}

/*  Copy a mission's title or location into buf, NUL terminated. */
void missionTitle(const u8 mission, char *buf, const u16 bufLen);
void missionLocation(const u8 mission, char *buf, const u16 bufLen);
void missionMapPos(const u8 mission, u16 *x, u16 *y);

const char *difficultyName(const u8 d);

/*  Enemy damage and accuracy scaled for the current difficulty: the two
    numbers BALANCE.md names as the ones to move. */
u8 difficultyDamage(const u8 damage);
u8 difficultyAccuracy(const u8 accuracy);

#endif
