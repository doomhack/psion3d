#include "fp_types.h"
#include "level.h"
#include "game_map.h"
#include "decor.h"
#include "enemy.h"
#include "mission.h"
#include "pickup.h"

level_event_fn levelEvent = levelEventNone;

/* A level with no script: nothing is handled, so every event takes the
   default. */
u16 levelEventNone(const u8 event, const u8 item, const u8 x, const u8 y)
{
	return FALSE;
}

/* Map 1: the front company's laboratory. There are two ways into the lab:
   the expected route is the keycard in the office block, which opens the
   locked door off the inner lobby; the bypass is the vent shaft in the
   locker room, which skips the offices and the keycard. Both come in
   through the lab door at (20,26), so the second keycard at (19,26) is on
   the one cell every player crosses - picking it up completes objective 1
   and opens the locked doors, which gives the vent route a short walk back
   to the exit. Objective 2 is the rootkit on the research director's
   computer at (7,32); other computers do nothing. Objective 3 is not
   harming the staff. The office keycard takes the default and opens every
   locked door. */
#define MAP1_OBJECTIVE_LAB_ACCESS 0
#define MAP1_OBJECTIVE_ROOTKIT 1
#define MAP1_OBJECTIVE_STAFF 2

u16 levelEventMap1(const u8 event, const u8 item, const u8 x, const u8 y)
{
	switch(event)
	{
        case LEVEL_EVENT_PICKUP:
        {
            if(item == PICKUP_TYPE_KEYCARD)
            {
                if(x == 19 && y == 26)
                {
                    missionSetObjective(MAP1_OBJECTIVE_LAB_ACCESS, OBJECTIVE_COMPLETE);
                    unlockDoors(); //Unlock the lab door and exit door.
                    return TRUE;
                }
            }
        }
        break;

		case LEVEL_EVENT_USE_DECOR:
        if( (item == DECOR_TYPE_COMPUTER) && (x == 7) && (y == 32) )
			{
				missionSetObjective(MAP1_OBJECTIVE_ROOTKIT, OBJECTIVE_COMPLETE);
				return TRUE;
			}
			break;

		case LEVEL_EVENT_KILL:
			if(item == ENEMY_TYPE_CIV)
			{
				missionSetObjective(MAP1_OBJECTIVE_STAFF, OBJECTIVE_FAILED);
				return TRUE;
			}
			break;

		case LEVEL_EVENT_EXIT:
			/* Not harming the staff is only known to have held once the level
			   is over, so it completes here - unless a kill already failed it. */
			if(objectiveState[MAP1_OBJECTIVE_STAFF] == OBJECTIVE_INCOMPLETE)
				missionSetObjective(MAP1_OBJECTIVE_STAFF, OBJECTIVE_COMPLETE);

			return TRUE;
	}

	return FALSE;
}
