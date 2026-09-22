#ifndef MENU_H
#define MENU_H

#include "fp_types.h"

/*  The menu screens: the front end (main, mission select, briefing,
    objectives) and the pause set (pause, abort, objectives, map). One screen
    is open at a time; gameloop.c decides what its actions mean. Everything
    is drawn through ui.h, so this module is the same on the device and the
    PC host. */

#define MENU_MAIN 0
#define MENU_SELECT 1
#define MENU_BRIEFING 2
#define MENU_OBJECTIVES 3
#define MENU_PAUSE 4
#define MENU_ABORT 5
#define MENU_PAUSE_OBJECTIVES 6
#define MENU_MAP 7
#define MENU_OUTCOME 8	/* how the mission ended, from missionOutcome */
#define MENU_OPTIONS 9
#define MENU_BENCH 10	/* the benchmark results, from bench.c */
#define MENU_CHEATS 11

/*  What a key did. REDRAW covers every change that stays inside the menus;
    the others are for gameloop.c to act on. */
#define MENU_ACTION_NONE 0
#define MENU_ACTION_REDRAW 1
#define MENU_ACTION_START_MISSION 2
#define MENU_ACTION_RESUME 3
#define MENU_ACTION_ABORT 4
#define MENU_ACTION_QUIT 5
#define MENU_ACTION_RETRY 6	/* the same mission again, from the outcome screen */
#define MENU_ACTION_BENCH 7	/* run the benchmark, from Options or the results screen */

/*  The mission highlighted in the list, and so the one a START_MISSION
    action means. Index into the mission index, not a map id. */
extern u8 menuMission;

void menuOpen(const u8 screen);
u16 menuKey(const u16 uiKey);
void menuDraw(void);

#endif
