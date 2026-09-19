#include <plib.h>
#include <wlib.h>

#include "ui.h"
#include "ui_psion.h"
#include "bitmap.h"
#include "menu.h"

/*  The device side of ui.h: a full-screen window over the game and debug
    windows, drawn with WLIB. It is shown for the menus and hidden while
    playing, when blitVideoMem writes straight to video RAM underneath it.

    Fonts are the ROM Swiss 13 and Swiss 16 (fonts.h: FONT_ID_SWISS_13 is
    WS_FONT_BASE + 0xa), which is what the design's 13px and 16px Helvetica
    are. Bold is the server's synthesised G_STY_BOLD. */

#define FONT_SWISS_13 (WS_FONT_BASE + 0xa)
#define FONT_SWISS_16 (WS_FONT_BASE + 0xb)

static const P_RECT menuWinRect = {{0, 0}, {UI_W, UI_H}};

static UINT menuWindowId = 0;
static UINT menuGc = 0;
static UINT menuGcGrey = 0;
static u16 menuShown = FALSE;

/*  The font currently set on the black GC, so a run of text in one font
    costs one gSetGC rather than one per call. -1 forces the first set. */
static s16 curFont = -1;
static s16 curInverse = -1;

/*  Ascent per UI_FONT_*, read once from gFontInfo, so uiText can take the
    top of the line and gPrintText its baseline. */
static s16 fontAscent[3];

/*  uiBlitMap's route: the segment-backed WLIB bitmap the compatibility blit
    in psion3d.c also uses, created on first use so a session that never
    opens the map never pays for it. */
static UINT mapBitmap = 0;
static HANDLE mapBmHandle = 0;

static void fontFor(s16 font, UINT *handle, UINT *style)
{
	switch(font)
	{
	case UI_FONT_BODY_BOLD:
		*handle = FONT_SWISS_13;
		*style = G_STY_BOLD;
		break;
	case UI_FONT_HEAD_BOLD:
		*handle = FONT_SWISS_16;
		*style = G_STY_BOLD;
		break;
	default:
		*handle = FONT_SWISS_13;
		*style = G_STY_NORMAL;
		break;
	}
}

static void selectFont(s16 font, s16 inverse)
{
	G_GC gc;
	UINT handle, style;

	gSetGC0(menuGc);

	if(font == curFont && inverse == curInverse)
		return;

	fontFor(font, &handle, &style);

	gc.font = handle;
	gc.style = (u8)style;
	gc.textmode = inverse ? G_TRMODE_CLR : G_TRMODE_SET;

	gSetGC(menuGc, G_GC_MASK_FONT | G_GC_MASK_STYLE | G_GC_MASK_TEXTMODE, &gc);

	curFont = font;
	curInverse = inverse;
}

static void setRect(P_RECT *r, s16 x, s16 y, s16 w, s16 h)
{
	r->tl.x = x;
	r->tl.y = y;
	r->br.x = x + w;
	r->br.y = y + h;
}

void createMenuWindow(void)
{
	W_WINDATA windata;
	G_GC ggc;
	G_FONT_INFO info;
	UINT handle, style;
	s16 i;

	windata.flags = W_WIN_PRIORITY;
	windata.extent.tl.x = 0;
	windata.extent.tl.y = 0;
	windata.extent.width = UI_W;
	windata.extent.height = UI_H;
	windata.background = W_WIN_BACK_CLR | W_WIN_BACK_GREY_CLR;

	menuWindowId = wCreateWindow(0, W_WIN_EXTENT | W_WIN_BACKGROUND, &windata, MENU_WIN);
	wInitialiseWindowTree(menuWindowId);

	menuGc = gCreateGC0(menuWindowId);

	ggc.flags = G_GC_FLAG_GREY_PLANE;
	menuGcGrey = gCreateGC(menuWindowId, G_GC_MASK_GREY, &ggc);

	for(i = 0; i < 3; i++)
	{
		fontFor(i, &handle, &style);
		info.ascent = 0;
		gFontInfo(handle, style, &info);
		fontAscent[i] = (s16)info.ascent;
	}

	menuShown = TRUE;
}

void menuWindowShow(u16 on)
{
	if(on == menuShown)
		return;

	menuShown = on;

	if(on)
	{
		wMakeVisible(menuWindowId);
		wInvalidateWin(menuWindowId);
	}
	else
	{
		wMakeInvisible(menuWindowId);
	}
}

void menuWindowInvalidate(void)
{
	wInvalidateWin(menuWindowId);
}

void menuWindowRedraw(void)
{
	/* Not wBeginRedrawWinGC0: that creates a temporary GC0, and while one
	   exists selecting a permanent GC (as every primitive here does, to
	   swap between the black and grey planes) is panic 101. The plain
	   variant only validates and clips; the permanent GCs stay usable
	   (Window Server Reference 1-26, 3-9). */
	wBeginRedrawWin(menuWindowId);

	/* The GC's font is unknown across redraws; force the first set. */
	curFont = -1;
	curInverse = -1;

	menuDraw();
	wEndRedraw();
}

/* ------------------------------------------------------------------ ui.h */

void uiClear(void)
{
	gSetGC0(menuGcGrey);
	gClrRect(&menuWinRect, G_TRMODE_CLR);
	gSetGC0(menuGc);
	gClrRect(&menuWinRect, G_TRMODE_CLR);
}

void uiFillRect(short x, short y, short w, short h)
{
	P_RECT r;

	if(w <= 0 || h <= 0)
		return;

	setRect(&r, x, y, w, h);
	gSetGC0(menuGc);
	gClrRect(&r, G_TRMODE_SET);
}

void uiClearRect(short x, short y, short w, short h)
{
	P_RECT r;

	if(w <= 0 || h <= 0)
		return;

	setRect(&r, x, y, w, h);
	gSetGC0(menuGcGrey);
	gClrRect(&r, G_TRMODE_CLR);
	gSetGC0(menuGc);
	gClrRect(&r, G_TRMODE_CLR);
}

void uiGreyRect(short x, short y, short w, short h)
{
	P_RECT r;

	if(w <= 0 || h <= 0)
		return;

	setRect(&r, x, y, w, h);
	gSetGC0(menuGcGrey);
	gClrRect(&r, G_TRMODE_SET);
	gSetGC0(menuGc);
}

void uiPatternRect(short x, short y, short w, short h)
{
	P_RECT r;

	if(w <= 0 || h <= 0)
		return;

	setRect(&r, x, y, w, h);
	gSetGC0(menuGc);
	gFillPattern(&r, WS_BITMAP_GREY, G_TRMODE_REPL);
}

void uiBox(short x, short y, short w, short h)
{
	P_RECT r;

	if(w <= 0 || h <= 0)
		return;

	setRect(&r, x, y, w, h);
	gSetGC0(menuGc);
	gDrawBox(&r);
}

void uiLine(short x0, short y0, short x1, short y1)
{
	P_RECT r;

	gSetGC0(menuGc);

	/* Axis-aligned lines are the common case, and a one pixel rectangle is
	   exact about both ends where gDrawLine's end point handling is not. */
	if(y0 == y1)
	{
		if(x1 < x0)
		{
			short t = x0;
			x0 = x1;
			x1 = t;
		}

		setRect(&r, x0, y0, (short)(x1 - x0 + 1), 1);
		gClrRect(&r, G_TRMODE_SET);
		return;
	}

	if(x0 == x1)
	{
		if(y1 < y0)
		{
			short t = y0;
			y0 = y1;
			y1 = t;
		}

		setRect(&r, x0, y0, 1, (short)(y1 - y0 + 1));
		gClrRect(&r, G_TRMODE_SET);
		return;
	}

	/* gDrawLine excludes its end pixel; the objective marks want it. */
	gDrawLine(x0, y0, x1, y1);
	setRect(&r, x1, y1, 1, 1);
	gClrRect(&r, G_TRMODE_SET);
}

void uiText(short x, short y, short font, short inverse, const char *s, short len)
{
	if(len <= 0)
		return;

	selectFont(font, inverse);

	/* gPrintText takes the baseline, the pixel row of the lowest ascent
	   pixel; y is the row's centre, so the capitals straddle it. Measured on
	   the device: Swiss 13 reports ascent 10 (glyphs reach 11 rows up), so
	   in a 20 row bar this puts the caps on rows 4..14. */
	gPrintText(x, y + (fontAscent[font] >> 1), s, len);
}

short uiTextWidth(short font, const char *s, short len)
{
	UINT handle, style;

	if(len <= 0)
		return 0;

	fontFor(font, &handle, &style);

	return (short)gTextWidth(handle, style, s, len);
}

void uiBlitMap(short dstX, short dstY, short srcY, short w, short h)
{
	P_POINT dst;
	P_RECT src;

	if(mapBitmap == 0)
	{
		W_OPEN_BIT_SEG bmSeg;

		bmSeg.size.x = 256;
		bmSeg.size.y = 320;
		mapBitmap = gCreateBit(WS_BIT_SEG_ACCESS, &bmSeg);
		mapBmHandle = p_sgopen(bmSeg.seg_name);
	}

	p_sgcopyto(mapBmHandle, 0, &screenBm[0], BM_SCREEN_BYTES);

	dst.x = dstX;
	dst.y = dstY;

	setRect(&src, 0, srcY, w, h);
	gSetGC0(menuGc);
	gCopyBit(&dst, mapBitmap, &src, G_TRMODE_REPL);

	/* The grey plane is the bottom half of the same bitmap. */
	src.tl.y += 160;
	src.br.y += 160;
	gSetGC0(menuGcGrey);
	gCopyBit(&dst, mapBitmap, &src, G_TRMODE_REPL);
	gSetGC0(menuGc);
}
