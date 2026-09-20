#include <plib.h>
#include <wlib.h>

#include "psion3d.h"
#include "draw.h"
#include "bitmap.h"
#include "debug.h"
#include "enemy.h"
#include "player.h"
#include "units.h"
#include "videomem.h"
#include "gameloop.h"
#include "ui.h"
#include "ui_psion.h"
#include "hud.h"

/*  Were in psion3d.h until it was made SDK-free; only this file reads them. */
static const P_RECT gameWinRect = {{0,0}, {240,160}};
static const P_RECT gameBitmapRect = {{0,0}, {256,320}};

static WSERV_SPEC wSpec;
static UINT gameWindowId = 0;

static UINT bitmap = 0;
static HANDLE bmHandle = 0;

static UINT wgc[2]  = {0};

const INT GAME_WIN = 2;

#define DIRECT_VIDEO_MEM_ACCESS


static void updateScreen()
{
#ifdef DIRECT_VIDEO_MEM_ACCESS

	blitVideoMem(blackBm, greyBm);

#else

	const P_RECT greyRect = {{0,160}, {240,320}};

	p_sgcopyto(bmHandle, 0, &screenBm[0], BM_SCREEN_BYTES);

	gSetGC0(wgc[BM_BLK]);
	gCopyBit(&gameWinRect.tl, bitmap, &gameWinRect, G_TRMODE_REPL);

	gSetGC0(wgc[BM_GRY]);
	gCopyBit(&gameWinRect.tl, bitmap, &greyRect, G_TRMODE_REPL);

#endif

}

static void updateKeys()
{
	UWORD kbScan[10];

	keys = 0;

	p_getscancodes(kbScan);

	//Arrows move and turn; W/S move and A/D strafe, as on the PC host.
	if((kbScan[7] & 0x20) || (kbScan[6] & 0x20))
		keys |= KEY_UP;

	if(kbScan[2] & 0x80)
		keys |= KEY_FIRE;

	if(kbScan[4] & 0x1)
		keys |= KEY_USE;

	if((kbScan[0] & 0x20) || (kbScan[6] & 0x10))
		keys |= KEY_DOWN;

	if(kbScan[0] & 0x10)
		keys |= KEY_LEFT;

	if(kbScan[0] & 0x2)
		keys |= KEY_RIGHT;

	if(kbScan[7] & 0x2)
		keys |= KEY_WEAPON_1;
	
	if(kbScan[7] & 0x4)
		keys |= KEY_WEAPON_2;

	if(kbScan[5] & 0x40)
		keys |= KEY_WEAPON_3;

	if(kbScan[4] & 0x4)
		keys |= KEY_WEAPON_4;

	if((kbScan[3] & 0x2) || (kbScan[6] & 0x4))
		keys |= KEY_STRAFE_LEFT;

	if((kbScan[7] & 0x10) || (kbScan[5] & 0x10))
		keys |= KEY_STRAFE_RIGHT;
}

static u16 runTicks(u16 gameTime)
{
	u16 realTime = p_returntickcount();

	/* The physical key state cannot change between catch-up ticks, so one
	   p_getscancodes per frame is enough rather than one per tick. */
	updateKeys();

	/* The catch-up loop and the render live in gameloop.c so that the PC
	   development build runs exactly the same code. Sampling input and
	   presenting the bitmap stay here, because both are platform. */
	gameTime = gameRunTicks(gameTime, realTime);

	/* A tick can end the mission. Then the outcome screen is open, and the
	   menu window comes up over the game instead of the frame going out. */
	if(gameMode != GAME_MODE_PLAYING)
		menuWindowShow(TRUE);
	else
		updateScreen();

	return gameTime;
}

/*  A window server key event to the UI_KEY_* the menus take. Everything else
    is UI_KEY_NONE, including the letters that drive the game, which are read
    as levels from the scancodes rather than as events. */
static u16 uiKeyFor(u16 keycode)
{
	keycode &= (u16)~W_KEY_REPEATED;

	switch(keycode)
	{
	case W_KEY_UP:
		return UI_KEY_UP;
	case W_KEY_DOWN:
		return UI_KEY_DOWN;
	case W_KEY_LEFT:
		return UI_KEY_LEFT;
	case W_KEY_RIGHT:
		return UI_KEY_RIGHT;
	case W_KEY_RETURN:
		return UI_KEY_ENTER;
	case W_KEY_ESCAPE:
		return UI_KEY_ESC;
	case ' ':
		return UI_KEY_SPACE;
	}

	return UI_KEY_NONE;
}

static void mainLoop()
{
	WS_EV event;

	u16 frames = 0;
	u16 gameTime = p_returntickcount();
	u16 lastTick = gameTime, t = 0;
	u16 isForground = TRUE;
	u8 modeBefore;

	hudWindowInvalidate();

	while(1)
	{
		/* Only play polls: the frame loop below runs while the event is
		   pending. A menu waits for its next key without spinning. */
		if(isForground && gameMode == GAME_MODE_PLAYING)
			wGetEventSpecial(&event, WE_KEY | WE_REDRAW | WE_STATUS);
		else
			wGetEventWait(&event);

		while(event.type == E_FILE_PENDING)
		{
			/* The event request stays outstanding until it completes, so a
			   mission ending mid-loop cannot switch to wGetEventWait here:
			   the spin goes on, without frames, until the menu window's own
			   redraw event (raised when it was shown) ends it. */
			if(gameMode == GAME_MODE_PLAYING)
				gameTime = runTicks(gameTime);

			wFlush();

			frames++;

			t = p_returntickcount();

			if(tickElapsed(t, lastTick) >= TICKS_PER_SECOND)
			{
				hudSetFps((u8)(frames > 255 ? 255 : frames));
				frames = 0;
				lastTick = t;
			}

			/* HUD cells that moved this frame are drawn straight into the
			   window, buffered with the frame's other calls: no invalidation
			   and no redraw event, so a shot costs its ammo cell and nothing
			   else. */
			if(gameMode == GAME_MODE_PLAYING)
			{
				uiTarget(UI_TARGET_HUD);
				hudUpdate();
			}
		}

		if (event.type == WM_KEY)
		{
			u16 key = uiKeyFor(event.p.key.keycode);

			if(key != UI_KEY_NONE)
			{
				u16 action;

				modeBefore = gameMode;
				action = gameKey(key);

				if(action == GAME_KEY_QUIT)
					p_exit(0);

				if(gameMode != modeBefore)
				{
					/* Into play: drop the menu window so the server repaints
					   the game window under it, and restart the clock so the
					   time spent in the menu is not caught up in one frame.
					   Out of play: the menu comes up and paints itself. */
					menuWindowShow(gameMode == GAME_MODE_MENU);
					gameTime = p_returntickcount();
				}
				else if(action == GAME_KEY_REDRAW)
				{
					menuWindowInvalidate();
				}
			}
		}
		else if (event.type == WM_REDRAW)
		{
			if(event.handle == MENU_WIN)
			{
				menuWindowRedraw();
			}
			else if(event.handle == HUD_WIN)
			{
				hudWindowRedraw();
			}
			else if(event.handle == GAME_WIN)
			{
				wValidateWin(gameWindowId);

				/* The direct blit ignores window clipping, so it must not
				   run while the menu window is over the game window. */
				if(gameMode == GAME_MODE_PLAYING)
					updateScreen();
			}
		}
		else if(event.type == WM_BACKGROUND)
		{
			isForground = FALSE;
		}
		else if(event.type == WM_FOREGROUND)
		{
			isForground = TRUE;
			gameTime = p_returntickcount();
		}
	}
}

static void createGameWindow()
{
	W_WINDATA windata;
	G_GC ggc;

#ifndef DIRECT_VIDEO_MEM_ACCESS
	W_OPEN_BIT_SEG bmSeg;

	bmSeg.size = gameBitmapRect.br;
	bitmap = gCreateBit(WS_BIT_SEG_ACCESS, &bmSeg);
	bmHandle = p_sgopen(bmSeg.seg_name);
#endif

	windata.flags = W_WIN_PRIORITY;
	windata.extent.tl.x = 120;
	windata.extent.tl.y = 0;
	windata.extent.width = gameWinRect.br.x;
	windata.extent.height = gameWinRect.br.y;
	windata.background = W_WIN_BACK_NONE | W_WIN_BACK_GREY_NONE;

	gameWindowId = wCreateWindow(0, W_WIN_EXTENT | W_WIN_BACKGROUND, &windata, GAME_WIN);
		
	wInitialiseWindowTree(gameWindowId);
	
	wgc[BM_BLK] = gCreateGC0(gameWindowId);
	
	ggc.flags = G_GC_FLAG_GREY_PLANE;
	wgc[BM_GRY] = gCreateGC(gameWindowId, G_GC_MASK_GREY, &ggc);
	
}

void main()
{
	wConnect(&wSpec, 0, W_CONNECT_PRIORITY);
	wCompatibilityMode(0, &wSpec);

	/* Bottom of the stack: the HUD's side panels show either side of the
	   game window, which is created over its middle. */
	createHudWindow();

	createGameWindow();

	/* Created last, so it sits over the other two while it is visible. */
	createMenuWindow();

	gameInit();
	menuWindowInvalidate();

	mainLoop();
}
