#include "gameloop.h"

#include "bitmap.h"
#include "draw.h"
#include "enemy.h"
#include "player.h"
#include "game_map.h"
#include "mission.h"
#include "menu.h"
#include "ui.h"
#include "hud.h"
#include "bench.h"

u16 keys = 0;
u8 gameMode = GAME_MODE_MENU;

/*  The level in play, for a retry from the outcome screen. */
static u8 currentMapId = 0;

s16 tickDelta(const u16 later, const u16 earlier)
{
	return (s16)(later - earlier);
}

u16 tickElapsed(const u16 later, const u16 earlier)
{
	return (u16)(later - earlier);
}

void gameInit(void)
{
	missionScan();
	gameMode = GAME_MODE_MENU;
	menuOpen(MENU_MAIN);
}

u16 gameStartMission(const u8 mapId)
{
	if(!loadMap(mapId))
		return FALSE;

	missionStart(mapId);
	initPlayer();
	hudInvalidate();
	keys = 0;
	currentMapId = mapId;
	gameMode = GAME_MODE_PLAYING;

	return TRUE;
}

u16 gameKey(const u16 uiKey)
{
	u16 action;

	if(gameMode == GAME_MODE_PLAYING)
	{
		if(uiKey != UI_KEY_ESC)
			return GAME_KEY_IGNORED;

		keys = 0;
		gameMode = GAME_MODE_MENU;

		/* Esc abandons a benchmark run and shows what it measured so far. */
		if(benchActive)
		{
			benchStop();
			menuOpen(MENU_BENCH);
		}
		else
		{
			menuOpen(MENU_PAUSE);
		}

		return GAME_KEY_REDRAW;
	}

	action = menuKey(uiKey);

	switch(action)
	{
	case MENU_ACTION_NONE:
		return GAME_KEY_IGNORED;

	case MENU_ACTION_START_MISSION:
		/* A level that fails to load leaves the menu where it is. */
		if(gameStartMission(missionMapId(menuMission)))
			return GAME_KEY_IGNORED;

		return GAME_KEY_REDRAW;

	case MENU_ACTION_RETRY:
		if(gameStartMission(currentMapId))
			return GAME_KEY_IGNORED;

		return GAME_KEY_REDRAW;

	case MENU_ACTION_RESUME:
		gameMode = GAME_MODE_PLAYING;
		return GAME_KEY_IGNORED;

	case MENU_ACTION_ABORT:
		menuOpen(MENU_SELECT);
		return GAME_KEY_REDRAW;

	case MENU_ACTION_QUIT:
		return GAME_KEY_QUIT;

	case MENU_ACTION_BENCH:
		/* Into play at the first station; a failed load stays on the menu. */
		if(benchStart())
			return GAME_KEY_IGNORED;

		return GAME_KEY_REDRAW;
	}

	return GAME_KEY_REDRAW;
}

u16 gameRunTicks(u16 gameTime, const u16 realTime)
{
	/* A benchmark frame renders a frozen world: no input, no AI and no
	   mission clock, so the frame rate is the renderer's alone and the
	   station looks the same in every frame. */
	if(benchActive)
	{
		bmClearScreen();
		draw();
		benchFrame(realTime);

		return realTime;
	}

	while(tickDelta(realTime, gameTime) > 0)
	{
		/* A dead player takes no input; the last hit plays out on its own. */
		updatePlayer(player.health ? keys : 0);
		runAI();

		gameTime++;

		if(missionTick() != OUTCOME_NONE)
		{
			/* The mission is over. The platform sees the mode change after
			   this frame and brings the menu window up on the outcome. */
			keys = 0;
			gameMode = GAME_MODE_MENU;
			menuOpen(MENU_OUTCOME);
			break;
		}
	}

	bmClearScreen();
	draw();

	return gameTime;
}
