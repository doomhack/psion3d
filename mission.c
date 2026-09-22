#include <plib.h>	/* the index lives in a far segment: p_sgcreate / p_sgcopyto */

#include "mission.h"
#include "player.h"
#include "ui.h"	/* uiInfoMsg, for the objective notices */
#include "cheat.h"

u8 missionCount = 0;
u8 difficulty = DIFFICULTY_SENIOR;
u8 objectiveState[MAP_MAX_OBJECTIVES];
u8 missionIndex = MISSION_NONE;
u16 missionTicks = 0;
u8 missionOutcome = OUTCOME_NONE;
u8 missionNewBest = FALSE;

/*  Ticks between health reaching zero and the outcome screen, so the last
    hit's flash and shove are seen before the screen changes. */
#define DEATH_TICKS 16

static u8 deathTicks = 0;

/*  One record per mission in the MISIDX segment: title, location, map
    position and a best time per difficulty. 20 x 74 bytes is 1,480 bytes
    and none of it is near data. */
#define MISSION_REC_TITLE 0
#define MISSION_REC_LOCATION MISSION_NAME_LEN
#define MISSION_REC_MAPPOS (MISSION_NAME_LEN * 2)
#define MISSION_REC_BEST (MISSION_NAME_LEN * 2 + 4)
#define MISSION_REC_BYTES (MISSION_NAME_LEN * 2 + 4 + DIFFICULTY_COUNT * 2)
#define MISSION_SEG_PARAS ((MISSION_MAX * MISSION_REC_BYTES + 15) / 16)

static HANDLE missionSeg = 0;

static const char *difficultyNames[DIFFICULTY_COUNT] =
{
	"Agent", "Senior", "Elite"
};

static long recOfs(const u8 mission, const u16 field)
{
	return (long)mission * MISSION_REC_BYTES + field;
}

/*  Build the index. Each level file is parsed by loadMapFile, which writes
    the grid into map[][] and the text into the level text segment as a real
    load would; nothing is playing yet, so both are scratch here. */
void missionScan(void)
{
	char buf[MISSION_NAME_LEN];
	u16 pos[2];
	u16 best;
	u8 i, d;

	missionCount = 0;

	if (missionSeg > 0)
		p_sgclose(missionSeg);

	missionSeg = p_sgcreate("MISIDX", MISSION_SEG_PARAS, E_SEGMENT_HIGH);

	if (missionSeg <= 0)
		return;

	for (i = 0; i < MISSION_MAX; i++)
	{
		if (!loadMapFile(missionMapId(i)))
			break;

		mapTextCopy(mapInfo.titleOfs, buf, sizeof(buf));
		p_sgcopyto(missionSeg, recOfs(i, MISSION_REC_TITLE), buf, sizeof(buf));

		mapTextCopy(mapInfo.locationOfs, buf, sizeof(buf));
		p_sgcopyto(missionSeg, recOfs(i, MISSION_REC_LOCATION), buf, sizeof(buf));

		pos[0] = mapInfo.mapPosX;
		pos[1] = mapInfo.mapPosY;
		p_sgcopyto(missionSeg, recOfs(i, MISSION_REC_MAPPOS), pos, sizeof(pos));

		for (d = 0; d < DIFFICULTY_COUNT; d++)
		{
			best = MISSION_TIME_NONE;
			p_sgcopyto(missionSeg, recOfs(i, (u16)(MISSION_REC_BEST + d * 2)), &best, sizeof(best));
		}

		missionCount++;
	}
}

void missionStart(const u8 mapId)
{
	u8 i;

	missionIndex = (mapId >= 1 && mapId <= missionCount) ? (u8)(mapId - 1) : MISSION_NONE;
	missionTicks = 0;
	missionOutcome = OUTCOME_NONE;
	missionNewBest = FALSE;
	deathTicks = 0;

	for (i = 0; i < MAP_MAX_OBJECTIVES; i++)
		objectiveState[i] = OBJECTIVE_INCOMPLETE;
}

/* Copy s over buf at *n and step *n past it. No terminator: the caller adds
   one after the last piece. The message is assembled by hand rather than
   with p_atos so the PC build, which has no plib formatting, gets the same
   text. */
static void msgAppend(char *buf, u8 *n, const char *s)
{
	while (*s)
		buf[(*n)++] = *s++;
}

void missionSetObjective(const u8 i, const u8 state)
{
	/* "Objective N Completed" is 21 bytes; room for the terminator and the
	   longer word either way. */
	char msg[24];
	u8 n = 0;

	if (i >= MAP_MAX_OBJECTIVES || objectiveState[i] == state)
		return;

	objectiveState[i] = state;

	if (state == OBJECTIVE_INCOMPLETE)
		return;

	msgAppend(msg, &n, "Objective ");
	msg[n++] = (char)('1' + i);
	msgAppend(msg, &n, (state == OBJECTIVE_COMPLETE) ? " Completed" : " Failed");
	msg[n] = 0;

	uiInfoMsg(msg);
}

u16 missionBest(const u8 mission, const u8 d)
{
	u16 best;

	if (mission >= missionCount || d >= DIFFICULTY_COUNT || missionSeg <= 0)
		return MISSION_TIME_NONE;

	p_sgcopyfr(missionSeg, recOfs(mission, (u16)(MISSION_REC_BEST + d * 2)), &best, sizeof(best));

	return best;
}

static void setBest(const u8 mission, const u8 d, u16 ticks)
{
	if (mission >= missionCount || d >= DIFFICULTY_COUNT || missionSeg <= 0)
		return;

	p_sgcopyto(missionSeg, recOfs(mission, (u16)(MISSION_REC_BEST + d * 2)), &ticks, sizeof(ticks));
}

/*  The mission is over: record how, and a completion's time if it beats
    the best so far. A run with any cheat on records nothing. */
static u8 endMission(const u8 outcome)
{
	missionOutcome = outcome;

	if (outcome == OUTCOME_COMPLETE && missionIndex != MISSION_NONE && !cheatActive)
	{
		const u16 best = missionBest(missionIndex, difficulty);

		if (best == MISSION_TIME_NONE || missionTicks < best)
		{
			setBest(missionIndex, difficulty, missionTicks);
			missionNewBest = TRUE;
		}
	}

	return outcome;
}

u8 missionTick(void)
{
	u8 i;

	if (missionTicks < MISSION_TIME_NONE - 1)
		missionTicks++;

	if (player.health == 0)
	{
		/* hurtPlayer has clamped to zero; give the last hit its frames. */
		if (++deathTicks >= DEATH_TICKS)
			return endMission(OUTCOME_KIA);

		return OUTCOME_NONE;
	}

	/* The end cell is judged the tick the player is on it, after the level
	   script has had its LEVEL_EVENT_EXIT (checkExit runs in updatePlayer,
	   ahead of this): every objective complete is the mission complete,
	   anything failed or still open is the mission failed. A failure earlier
	   in the mission does not end it - the player plays on to the exit and
	   learns the outcome there, as the objectives screen has been telling
	   them all along. */
	if (fp2int(player.pos.x) == mapInfo.endX && fp2int(player.pos.y) == mapInfo.endY)
	{
		for (i = 0; i < mapInfo.objectiveCount; i++)
			if (objectiveState[i] != OBJECTIVE_COMPLETE)
				return endMission(OUTCOME_FAILED);

		return endMission(OUTCOME_COMPLETE);
	}

	return OUTCOME_NONE;
}

static void copyField(const u8 mission, const u16 field, char *buf, const u16 bufLen)
{
	char rec[MISSION_NAME_LEN];
	u16 i;

	if (bufLen == 0)
		return;

	if (mission >= missionCount || missionSeg <= 0)
	{
		buf[0] = 0;
		return;
	}

	p_sgcopyfr(missionSeg, recOfs(mission, field), rec, sizeof(rec));
	rec[MISSION_NAME_LEN - 1] = 0;

	for (i = 0; i < bufLen - 1 && i < MISSION_NAME_LEN && rec[i]; i++)
		buf[i] = rec[i];

	buf[i] = 0;
}

void missionTitle(const u8 mission, char *buf, const u16 bufLen)
{
	copyField(mission, MISSION_REC_TITLE, buf, bufLen);
}

void missionLocation(const u8 mission, char *buf, const u16 bufLen)
{
	copyField(mission, MISSION_REC_LOCATION, buf, bufLen);
}

void missionMapPos(const u8 mission, u16 *x, u16 *y)
{
	u16 pos[2];

	*x = 0;
	*y = 0;

	if (mission >= missionCount || missionSeg <= 0)
		return;

	p_sgcopyfr(missionSeg, recOfs(mission, MISSION_REC_MAPPOS), pos, sizeof(pos));
	*x = pos[0];
	*y = pos[1];
}

const char *difficultyName(const u8 d)
{
	return difficultyNames[d < DIFFICULTY_COUNT ? d : DIFFICULTY_SENIOR];
}

/*  Agent takes a quarter off both, Elite adds a quarter. First guesses:
    BALANCE.md tunes Senior, and these are ratios to it. */
u8 difficultyDamage(const u8 damage)
{
	u16 v = damage;

	if (difficulty == DIFFICULTY_AGENT)
		v = (u16)(v * 3 / 4);
	else if (difficulty == DIFFICULTY_ELITE)
		v = (u16)(v * 5 / 4);

	if (v > 255)
		v = 255;

	return (u8)v;
}

u8 difficultyAccuracy(const u8 accuracy)
{
	u16 v = accuracy;

	if (difficulty == DIFFICULTY_AGENT)
		v = (u16)(v * 3 / 4);
	else if (difficulty == DIFFICULTY_ELITE)
		v = (u16)(v * 5 / 4);

	if (v > 255)
		v = 255;

	return (u8)v;
}
