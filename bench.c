#include "bench.h"
#include "gameloop.h"
#include "player.h"
#include "menu.h"

#ifdef BENCH_PROFILE
#include "bitmap.h"
#include "draw.h"
#endif

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

#ifdef BENCH_PROFILE

/*  The passes a station is measured in, and what each switches. The parts
    are differences between them (benchPartMs10), each against a pass that
    differs from it in that part alone. Walls cannot be switched off against
    the whole frame - a wall's result decides which sprites it hides - so
    they are drawn twice instead, and the rays are measured with everything
    that reads that result switched off too. */
#define PASS_FULL 0
#define PASS_NO_CLEAR 1
#define PASS_NO_BLIT 2
#define PASS_NO_WEAPON 3
#define PASS_NO_SPRITES 4
#define PASS_WALLS_TWICE 5
#define PASS_RAYS_ONLY 6
#define PASS_NO_DRAW 7

static const u16 passSkip[BENCH_PASSES] =
{
	0,
	BENCH_NO_CLEAR,
	BENCH_NO_BLIT,
	BENCH_NO_WEAPON,
	BENCH_NO_SPRITES,
	BENCH_WALLS_TWICE,
	BENCH_NO_WALLS | BENCH_NO_SPRITES | BENCH_NO_WEAPON,
	BENCH_NO_DRAW
};

u16 benchSkip = 0;

static u8 pass = 0;
static u16 passFrames[MAP_MAX_STATIONS][BENCH_PASSES];
static u16 passTicks[MAP_MAX_STATIONS][BENCH_PASSES];

#define PASS_TICKS BENCH_PASS_TICKS

#else

#define PASS_TICKS BENCH_STATION_TICKS

#endif

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

#ifdef BENCH_PROFILE

/*  Ticks per frame as tenths of a millisecond, rounded: a tick is 312.5
    tenths. */
static s16 passMs10(const u8 i, const u8 p)
{
	const u16 n = passFrames[i][p];

	if(n == 0)
		return 0;

	return (s16)(((s32)passTicks[i][p] * 3125 + n * 5) / ((s32)n * 10));
}

void benchDraw(void)
{
	if(!BENCH_SKIP(BENCH_NO_CLEAR))
		bmClearScreen();

	if(!BENCH_SKIP(BENCH_NO_DRAW))
		draw();
}

s16 benchPartMs10(const u8 i, const u8 part)
{
	s16 ms[BENCH_PASSES];
	s16 clear, blit, walls, sprites, weapon;
	u8 p;

	for(p = 0; p < BENCH_PASSES; p++)
		ms[p] = passMs10(i, p);

	clear = ms[PASS_FULL] - ms[PASS_NO_CLEAR];
	blit = ms[PASS_FULL] - ms[PASS_NO_BLIT];
	weapon = ms[PASS_FULL] - ms[PASS_NO_WEAPON];
	sprites = ms[PASS_FULL] - ms[PASS_NO_SPRITES];
	walls = ms[PASS_WALLS_TWICE] - ms[PASS_FULL];

	switch(part)
	{
	case BENCH_PART_FRAME:
		return ms[PASS_FULL];
	case BENCH_PART_RAYS:
		return ms[PASS_RAYS_ONLY] - ms[PASS_NO_DRAW];
	case BENCH_PART_WALLS:
		return walls;
	case BENCH_PART_SPRITES:
		return sprites;
	case BENCH_PART_WEAPON:
		return weapon;
	case BENCH_PART_CLEAR:
		return clear;
	case BENCH_PART_BLIT:
		return blit;
	case BENCH_PART_OTHER:
		/* The no-draw pass still clears and blits. */
		return ms[PASS_NO_DRAW] - clear - blit;
	}

	/* Rays, other, clear and blit sum to the rays-only pass, so what is
	   left of the frame after them and the three parts that pass leaves out
	   is whatever the switches failed to separate. */
	return ms[PASS_FULL] - ms[PASS_RAYS_ONLY] - walls - sprites - weapon;
}

#endif

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

#ifdef BENCH_PROFILE
	pass = 0;
	benchSkip = 0;
#endif

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

#ifdef BENCH_PROFILE
	benchSkip = 0;
#endif
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

	if(elapsed < PASS_TICKS)
		return;

#ifdef BENCH_PROFILE
	passFrames[station][pass] = frames;
	passTicks[station][pass] = elapsed;

	/* The next pass from the same place. Its first frame is the warm-up
	   again, so the frame that switched is not counted in either. */
	if(++pass < BENCH_PASSES)
	{
		benchSkip = passSkip[pass];
		warm = FALSE;
		return;
	}

	pass = 0;
	benchSkip = 0;

	/* The full pass is the station's frame rate, as the normal build has it. */
	frames = passFrames[station][PASS_FULL];
	elapsed = passTicks[station][PASS_FULL];
#endif

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
