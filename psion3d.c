#include "psion3d.h"
#include "draw.h"
#include "bitmap.h"
#include "debug.h"
#include "enemy.h"
#include "player.h"
#include "units.h"
#include "videomem.h"

static WSERV_SPEC wSpec;
static UINT gameWindowId = 0;
static UINT debugWindowId = 0;

static UINT bitmap = 0;
static HANDLE bmHandle = 0;

static UINT wgc[2]  = {0};

const INT DEBUG_WIN = 1;
const INT GAME_WIN = 2;

u8 keys = 0;

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

	wFlush();

}

static void updateKeys()
{
	UWORD kbScan[10];

	keys = 0;

	p_getscancodes(kbScan);

	if(kbScan[7] & 0x20)
		keys |= KEY_UP;

	if(kbScan[0] & 0x20)
		keys |= KEY_DOWN;

	if(kbScan[0] & 0x10)
		keys |= KEY_LEFT;

	if(kbScan[0] & 0x2)
		keys |= KEY_RIGHT;
}

static s16 tickDelta(const u16 later, const u16 earlier)
{
	return (s16)(later - earlier);
}

static void mainLoop()
{
	WS_EV event;
	
	u16 frames = 0;
	u16 lastTick = 0, t = 0;
	TEXT buf[32];
	u16 isForground = TRUE;

	u16 gameTime = p_returntickcount();
	u16 realTime;

	initPlayer();
		
	wInvalidateWin(debugWindowId);

	while(1)
	{
		if(isForground)
			wGetEventSpecial(&event, WE_REDRAW | WE_STATUS);
		else
			wGetEventWait(&event);

		while(event.type == E_FILE_PENDING)
		{
			realTime = p_returntickcount();

			while(tickDelta(realTime, gameTime) > 0)
			{
				updateKeys();
				updatePlayer(keys);
				runAI();

				gameTime++;
			}
			
			bmClearScreen();
			draw();
			updateScreen();
			
			frames++;
			
			t = p_returntickcount();
			
			if((u16)(t - lastTick) >= TICKS_PER_SECOND)
			{
				wInvalidateWin(debugWindowId);
				
				lastTick = t;
			}
		}
		
		if (event.type == WM_REDRAW)
		{
			if(event.handle == DEBUG_WIN)
			{
				wBeginRedrawWinGC0(debugWindowId);

				p_atos(&buf[0], "%d fps", frames);
				gPrintText(5, 20, &buf[0], p_slen(&buf[0]));
				
				p_atos(&buf[0], "Pos: %d,%d", pos.x, pos.y);
				gPrintText(5, 40, &buf[0], p_slen(&buf[0]));
				
				p_atos(&buf[0], "Angle: %d", pos.angle);
				gPrintText(5, 80, &buf[0], p_slen(&buf[0]));

				drawDbgText(5, 100);

				wEndRedraw();

				frames = 0;
			}
			else if(event.handle == GAME_WIN)
			{
				wValidateWin(gameWindowId);
				
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
	W_WINDATA windata = {0};

	wConnect(&wSpec, 0, W_CONNECT_PRIORITY);
	wCompatibilityMode(0, &wSpec);
	
	windata.flags=0;
	windata.extent.tl.x=0;
	windata.extent.tl.y=0;
	windata.extent.width=120;
	windata.extent.height=160;
	windata.background=W_WIN_BACK_CLR;
	
	debugWindowId = wCreateWindow(0, W_WIN_EXTENT | W_WIN_BACKGROUND, &windata, DEBUG_WIN);	
	wInitialiseWindowTree(debugWindowId);
	
	createGameWindow();

	loadMap(1);
	
	mainLoop();
}
