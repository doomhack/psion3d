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

extern u8 missionCount;
extern u8 difficulty;

/*  One state per mapInfo objective, OBJECTIVE_*. Reset at mission start;
    nothing advances them yet (TASKS.md task 3), so the pause screen shows
    every objective incomplete. */
extern u8 objectiveState[MAP_MAX_OBJECTIVES];

void missionScan(void);
void missionReset(void);

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
