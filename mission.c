#include <plib.h>	/* the index lives in a far segment: p_sgcreate / p_sgcopyto */

#include "mission.h"

u8 missionCount = 0;
u8 difficulty = DIFFICULTY_SENIOR;
u8 objectiveState[MAP_MAX_OBJECTIVES];

/*  One record per mission in the MISIDX segment: title, location, map
    position. 20 x 68 bytes is 1,360 bytes and none of it is near data. */
#define MISSION_REC_TITLE 0
#define MISSION_REC_LOCATION MISSION_NAME_LEN
#define MISSION_REC_MAPPOS (MISSION_NAME_LEN * 2)
#define MISSION_REC_BYTES (MISSION_NAME_LEN * 2 + 4)
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
	u8 i;

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

		missionCount++;
	}
}

void missionReset(void)
{
	u8 i;

	for (i = 0; i < MAP_MAX_OBJECTIVES; i++)
		objectiveState[i] = OBJECTIVE_INCOMPLETE;
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
