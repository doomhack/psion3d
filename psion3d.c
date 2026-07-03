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


static void updateScreen()
{
	const P_RECT greyRect = {{0,160}, {240,320}};

	p_sgcopyto(bmHandle, 0, &screenBm[0], BM_SCREEN_BYTES);

	gSetGC0(wgc[BM_BLK]);
	gCopyBit(&gameWinRect.tl, bitmap, &gameWinRect, G_TRMODE_REPL);

	gSetGC0(wgc[BM_GRY]);
	gCopyBit(&gameWinRect.tl, bitmap, &greyRect, G_TRMODE_REPL);

	wFlush();
}

static void testVidPtr()
{
	pokeVideoMem();
}

static void mainLoop()
{
	WS_EV event;
	
	u16 frames = 0, lastSecond = 0, t = 0;
	TEXT buf[32];

	initPlayer();
		
	wInvalidateWin(debugWindowId);

	while(1)
	{
		wGetEventSpecial(&event, WE_KEY | WE_REDRAW);

		while(event.type == E_FILE_PENDING)
		{
			updatePlayer();
			runAI();
			bmClearScreen();
			draw();
			updateScreen();
			testVidPtr();
			
			frames++;
			
			t = p_date();
			
			if(t != lastSecond)
			{
				wInvalidateWin(debugWindowId);
				
				lastSecond = t;
			}
			
			p_sleept(0);

			//p_sleept(SYSTEM_TICKS_PER_SECOND / TICKS_PER_SECOND);
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
		else if (event.type==WM_KEY)
		{
			handlePlayerKey(event.p.key.keycode);
		}
	}
}

static void createGameWindow()
{
	W_WINDATA windata;
	G_GC ggc;

	W_OPEN_BIT_SEG bmSeg;

	bmSeg.size = gameBitmapRect.br;
	bitmap = gCreateBit(WS_BIT_SEG_ACCESS, &bmSeg);
	bmHandle = p_sgopen(bmSeg.seg_name);

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
