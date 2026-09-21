#include "bench.h"
#include "gameloop.h"
#include "player.h"
#include "menu.h"

u8 benchActive = FALSE;
u8 benchDone = 0;
u16 benchFps10[MAP_MAX_STATIONS];
u16 benchAvg10 = 0;

/*  The station being measured and its clock. The first frame rendered at a
    station is the warm-up: it is drawn but not counted, and its tick starts
    the clock, so the frames counted after it span exactly the ticks measured. */
static u8 station = 0;
static u8 warm = FALSE;
static u16 startTick = 0;
static u16 frames = 0;
static u16 totalFrames = 0;
static u16 totalTicks = 0;

static void placeAtStation(const u8 i)
{
	const station_t *st = &mapInfo.stations[i];

	/* The middle of the cell, as initPlayer spawns. */
	player.pos.x = int2fp(st->x) + flt2fp(0.5f);
	player.pos.y = int2fp(st->y) + flt2fp(0.5f);
	player.pos.angle = st->f_angle;

	warm = FALSE;
}

/*  Frames per tick as tenths of a frame per second, rounded. One long divide
    per station, well away from the frame. */
static u16 fps10(const u16 nFrames, const u16 ticks)
{
	if(ticks == 0)
		return 0;

	return (u16)(((s32)nFrames * (10 * TICKS_PER_SECOND) + (ticks >> 1)) / ticks);
}

u16 benchStart(void)
{
	u8 i;

	benchActive = FALSE;
	benchDone = 0;
	benchAvg10 = 0;
	totalFrames = 0;
	totalTicks = 0;

	for(i = 0; i < MAP_MAX_STATIONS; i++)
		benchFps10[i] = 0;

	if(!gameStartMission(BENCH_MAP_ID) || mapInfo.stationCount == 0)
	{
		gameMode = GAME_MODE_MENU;
		return FALSE;
	}

	station = 0;
	placeAtStation(0);
	benchActive = TRUE;

	return TRUE;
}

void benchStop(void)
{
	benchActive = FALSE;
}

void benchFrame(const u16 realTime)
{
	u16 elapsed;

	if(!benchActive)
		return;

	if(!warm)
	{
		warm = TRUE;
		startTick = realTime;
		frames = 0;
		return;
	}

	frames++;
	elapsed = tickElapsed(realTime, startTick);

	if(elapsed < BENCH_STATION_TICKS)
		return;

	benchFps10[station] = fps10(frames, elapsed);
	totalFrames += frames;
	totalTicks += elapsed;
	benchDone = (u8)(station + 1);

	if(benchDone < mapInfo.stationCount)
	{
		station = benchDone;
		placeAtStation(station);
		return;
	}

	/* The run is over: the results screen comes up as an outcome would. */
	benchAvg10 = fps10(totalFrames, totalTicks);
	benchActive = FALSE;
	keys = 0;
	gameMode = GAME_MODE_MENU;
	menuOpen(MENU_BENCH);
}
