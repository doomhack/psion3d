#include "fp_types.h"
#include "level.h"
#include "game_map.h"
#include "decor.h"
#include "enemy.h"
#include "mission.h"

level_event_fn levelEvent = levelEventNone;

/* A level with no script: nothing is handled, so every event takes the
   default. */
u16 levelEventNone(const u8 event, const u8 item, const u8 x, const u8 y)
{
	return FALSE;
}

/* Map 1: the front company's laboratory. Objective 2 is the rootkit on the
   research director's computer and objective 3 is not harming the staff.
   Any computer completes the rootkit objective until the map settles on
   which desk is the director's - narrow it to that cell then. The keycard
   and the switch take the default and open every locked door. */
#define MAP1_OBJECTIVE_ROOTKIT 1
#define MAP1_OBJECTIVE_STAFF 2

u16 levelEventMap1(const u8 event, const u8 item, const u8 x, const u8 y)
{
	switch(event)
	{
		case LEVEL_EVENT_USE_DECOR:
			if(item == DECOR_TYPE_COMPUTER)
			{
				objectiveState[MAP1_OBJECTIVE_ROOTKIT] = OBJECTIVE_COMPLETE;
				return TRUE;
			}
			break;

		case LEVEL_EVENT_KILL:
			if(item == ENEMY_TYPE_CIV)
			{
				objectiveState[MAP1_OBJECTIVE_STAFF] = OBJECTIVE_FAILED;
				return TRUE;
			}
			break;
	}

	return FALSE;
}
