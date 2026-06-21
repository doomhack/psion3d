#include "psion3d.h"
#include "draw.h"

static WSERV_SPEC wSpec;
static UINT gameWindowId = 0;
static UINT debugWindowId = 0;

static UINT bitmaps[2] = {0};

static UINT wgc[2]  = {0};

const INT DEBUG_WIN = 1;
const INT GAME_WIN = 2;


UINT gc[2]  = {0};
position_t pos = {0};


f16 dbgval = 0;

//

static void clearScreen()
{
	gSetGC0(gc[BM_BLK]);
	gClrRect(&gameWinRect,G_TRMODE_CLR);
	
	gSetGC0(gc[BM_GRY]);
	gClrRect(&gameWinRect,G_TRMODE_CLR);
}

static void updateScreen()
{
	gSetGC0(wgc[BM_BLK]);
	gCopyBit(&gameWinRect.tl, bitmaps[BM_BLK], &gameWinRect, G_TRMODE_REPL);

	gSetGC0(wgc[BM_GRY]);
	gCopyBit(&gameWinRect.tl, bitmaps[BM_GRY], &gameWinRect, G_TRMODE_REPL);
}

static void tryMove(const f16 dx, const f16 dy)
{
	f16 nx = pos.x + dx;
	f16 ny = pos.y;
	
	s8 cell = fmapCell(nx, ny);
	
	if(canWalk(cell))
		pos.x = nx;
	
	nx = pos.x;
	ny = pos.y + dy;
	
	cell = fmapCell(nx, ny);
	
	if(canWalk(cell))
		pos.y = ny;
}

static void handleKey(const u16 key)
{
	if(key == W_KEY_LEFT)
	{
		pos.angle -= flt2fp(0.1f);
	}
	else if(key == W_KEY_RIGHT)
	{
		pos.angle += flt2fp(0.1f);
	}
	else if(key == W_KEY_UP)
	{		
		const f16 dx = fpmul(fpcos(pos.angle), flt2fp(0.2f));
		const f16 dy = fpmul(fpsin(pos.angle), flt2fp(0.2f));
		
		tryMove(dx, dy);
	}
	else if(key == W_KEY_DOWN)
	{		
		const f16 dx = -fpmul(fpcos(pos.angle), flt2fp(0.2f));
		const f16 dy = -fpmul(fpsin(pos.angle), flt2fp(0.2f));
		
		tryMove(dx, dy);
	}
	else if(key == W_KEY_ESCAPE)
	{
		
	}
}

static void mainLoop()
{
	WS_EV event;
	
	u16 frames = 0, lastSecond = 0, t = 0;
	TEXT buf[16];

	pos.x = flt2fp(27.5f);
	pos.y = flt2fp(1.5f);
	pos.angle = 0;
		
	wInvalidateWin(debugWindowId);

	while(1)
	{
		wGetEventSpecial(&event, WE_KEY | WE_REDRAW);

		while(event.type == E_FILE_PENDING)
		{
			clearScreen();
			draw();
			updateScreen();
			
			frames++;
			
			t = p_date();
			
			if(t != lastSecond)
			{
				wInvalidateWin(debugWindowId);
				
				lastSecond = t;
			}

			p_sleep(0);
		}
		
		if (event.type == WM_REDRAW)
		{
			if(event.handle == DEBUG_WIN)
			{
				wBeginRedrawWinGC0(debugWindowId);

				p_atos(&buf[0], "%d fps", frames);
				gPrintText(20, 20, &buf[0], p_slen(&buf[0]));
				
				
				p_atos(&buf[0], "X: %d", pos.x);
				gPrintText(20, 40, &buf[0], p_slen(&buf[0]));

				p_atos(&buf[0], "Y: %d", pos.y);
				gPrintText(20, 60, &buf[0], p_slen(&buf[0]));
				
				p_atos(&buf[0], "Angle: %d", pos.angle);
				gPrintText(20, 80, &buf[0], p_slen(&buf[0]));

				p_atos(&buf[0], "Dbg: %d", dbgval);
				gPrintText(20, 100, &buf[0], p_slen(&buf[0]));

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
			handleKey(event.p.key.keycode);
		}
	}
}

static void createGameWindow()
{
	W_WINDATA windata;
	G_GC ggc;

	bitmaps[BM_BLK] = gCreateBit(0, &gameWinRect.br);
	bitmaps[BM_GRY] = gCreateBit(0, &gameWinRect.br);

	gc[BM_BLK] = gCreateGC0(bitmaps[BM_BLK]);
	gc[BM_GRY] = gCreateGC0(bitmaps[BM_GRY]);

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
	
	mainLoop();
}
