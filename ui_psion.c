#include <plib.h>
#include <wlib.h>

#include "ui.h"
#include "ui_psion.h"
#include "bitmap.h"
#include "menu.h"
#include "hud.h"

/*  The device side of ui.h: two full-screen windows drawn with WLIB. The
    menu window sits over everything, shown for the menus and hidden while
    playing; the HUD window sits under the game window, created first, and
    only its side panels are ever drawn - blitVideoMem writes the game view
    straight to video RAM between them.

    Fonts are the ROM Swiss 13 and Swiss 16 (fonts.h: FONT_ID_SWISS_13 is
    WS_FONT_BASE + 0xa), which is what the design's 13px and 16px Helvetica
    are. Bold is the server's synthesised G_STY_BOLD. */

#define FONT_SWISS_13 (WS_FONT_BASE + 0xa)
#define FONT_SWISS_16 (WS_FONT_BASE + 0xb)

static const P_RECT fullRect = {{0, 0}, {UI_W, UI_H}};

/*  A drawable window: its id and a GC per plane. */
typedef struct uiwin_t
{
	UINT win;
	UINT gc;
	UINT gcGrey;
} uiwin_t;

static uiwin_t menuWin;
static uiwin_t hudWin;
static uiwin_t *cur = &menuWin;
static u16 menuShown = FALSE;

/*  The font currently set on the black GC, so a run of text in one font
    costs one gSetGC rather than one per call. -1 forces the first set. */
static s16 curFont = -1;
static s16 curInverse = -1;

/*  Ascent per UI_FONT_*, read once from gFontInfo, so uiText can take the
    top of the line and gPrintText its baseline. */
static s16 fontAscent[4];

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
	case UI_FONT_BIG:
		/* The server doubles every glyph row, which is exactly the
		   design's scaleY(2) on the 16px heading face. */
		*handle = FONT_SWISS_16;
		*style = G_STY_BOLD | G_STY_DOUBLE;
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

	gSetGC0(cur->gc);

	if(font == curFont && inverse == curInverse)
		return;

	fontFor(font, &handle, &style);

	gc.font = handle;
	gc.style = (u8)style;
	gc.textmode = inverse ? G_TRMODE_CLR : G_TRMODE_SET;

	gSetGC(cur->gc, G_GC_MASK_FONT | G_GC_MASK_STYLE | G_GC_MASK_TEXTMODE, &gc);

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

/*  A full-screen window with a GC per plane. Windows stack in creation
    order, so the caller decides what it sits over. */
static void createFullWindow(uiwin_t *w, const UINT handle)
{
	W_WINDATA windata;
	G_GC ggc;

	windata.flags = W_WIN_PRIORITY;
	windata.extent.tl.x = 0;
	windata.extent.tl.y = 0;
	windata.extent.width = UI_W;
	windata.extent.height = UI_H;
	windata.background = W_WIN_BACK_CLR | W_WIN_BACK_GREY_CLR;

	w->win = wCreateWindow(0, W_WIN_EXTENT | W_WIN_BACKGROUND, &windata, handle);
	wInitialiseWindowTree(w->win);

	w->gc = gCreateGC0(w->win);

	ggc.flags = G_GC_FLAG_GREY_PLANE;
	w->gcGrey = gCreateGC(w->win, G_GC_MASK_GREY, &ggc);
}

static void readFontMetrics(void)
{
	G_FONT_INFO info;
	UINT handle, style;
	s16 i;

	for(i = 0; i < 4; i++)
	{
		fontFor(i, &handle, &style);
		info.ascent = 0;
		gFontInfo(handle, style, &info);
		fontAscent[i] = (s16)info.ascent;
	}
}

void createHudWindow(void)
{
	createFullWindow(&hudWin, HUD_WIN);
	readFontMetrics();
}

void hudWindowInvalidate(void)
{
	wInvalidateWin(hudWin.win);
}

void hudWindowRedraw(void)
{
	wBeginRedrawWin(hudWin.win);
	uiTarget(UI_TARGET_HUD);
	hudDraw();
	wEndRedraw();
}

void createMenuWindow(void)
{
	createFullWindow(&menuWin, MENU_WIN);
	menuShown = TRUE;
}

void menuWindowShow(u16 on)
{
	if(on == menuShown)
		return;

	menuShown = on;

	if(on)
	{
		wMakeVisible(menuWin.win);
		wInvalidateWin(menuWin.win);
	}
	else
	{
		wMakeInvisible(menuWin.win);
	}
}

void menuWindowInvalidate(void)
{
	wInvalidateWin(menuWin.win);
}

void menuWindowRedraw(void)
{
	/* Not wBeginRedrawWinGC0: that creates a temporary GC0, and while one
	   exists selecting a permanent GC (as every primitive here does, to
	   swap between the black and grey planes) is panic 101. The plain
	   variant only validates and clips; the permanent GCs stay usable
	   (Window Server Reference 1-26, 3-9). */
	wBeginRedrawWin(menuWin.win);
	uiTarget(UI_TARGET_MENU);
	menuDraw();
	wEndRedraw();
}

/* ------------------------------------------------------------------ ui.h */

void uiTarget(short target)
{
	cur = (target == UI_TARGET_HUD) ? &hudWin : &menuWin;

	/* The new GC's font is unknown; force the first set. */
	curFont = -1;
	curInverse = -1;
}

void uiClear(void)
{
	gSetGC0(cur->gcGrey);
	gClrRect(&fullRect, G_TRMODE_CLR);
	gSetGC0(cur->gc);
	gClrRect(&fullRect, G_TRMODE_CLR);
}

void uiFillRect(short x, short y, short w, short h)
{
	P_RECT r;

	if(w <= 0 || h <= 0)
		return;

	setRect(&r, x, y, w, h);
	gSetGC0(cur->gc);
	gClrRect(&r, G_TRMODE_SET);
}

void uiClearRect(short x, short y, short w, short h)
{
	P_RECT r;

	if(w <= 0 || h <= 0)
		return;

	setRect(&r, x, y, w, h);
	gSetGC0(cur->gcGrey);
	gClrRect(&r, G_TRMODE_CLR);
	gSetGC0(cur->gc);
	gClrRect(&r, G_TRMODE_CLR);
}

void uiGreyRect(short x, short y, short w, short h)
{
	P_RECT r;

	if(w <= 0 || h <= 0)
		return;

	setRect(&r, x, y, w, h);
	gSetGC0(cur->gcGrey);
	gClrRect(&r, G_TRMODE_SET);
	gSetGC0(cur->gc);
}

void uiPatternRect(short x, short y, short w, short h)
{
	P_RECT r;

	if(w <= 0 || h <= 0)
		return;

	setRect(&r, x, y, w, h);
	gSetGC0(cur->gc);
	gFillPattern(&r, WS_BITMAP_GREY, G_TRMODE_REPL);
}

void uiBox(short x, short y, short w, short h)
{
	P_RECT r;

	if(w <= 0 || h <= 0)
		return;

	setRect(&r, x, y, w, h);
	gSetGC0(cur->gc);
	gDrawBox(&r);
}

void uiLine(short x0, short y0, short x1, short y1)
{
	P_RECT r;

	gSetGC0(cur->gc);

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

void uiTextBox(short x, short y, short w, short h, short font, short inverse, short align, const char *s, short len)
{
	G_GC gc;
	UINT handle, style;
	P_RECT r;
	UINT boxAlign;

	if(w <= 0 || h <= 0)
		return;

	/* gPrintBoxText replaces the whole box: the glyphs and, for the rest,
	   clear or set by the inverse bit of the style. So the one call does the
	   clearing, the filling and the aligning, and nothing is measured on our
	   side. The style it leaves on the GC is not what uiText wants, so the
	   font cache is dropped afterwards. */
	fontFor(font, &handle, &style);
	gc.font = handle;
	gc.style = (u8)(style | (inverse ? G_STY_INVERSE : 0));

	gSetGC0(cur->gc);
	gSetGC(cur->gc, G_GC_MASK_FONT | G_GC_MASK_STYLE, &gc);
	curFont = -1;
	curInverse = -1;

	setRect(&r, x, y, w, h);
	boxAlign = (align == UI_ALIGN_RIGHT) ? G_TEXT_ALIGN_RIGHT :
		(align == UI_ALIGN_CENTRE) ? G_TEXT_ALIGN_CENTRE : G_TEXT_ALIGN_LEFT;

	/* Capitals centred on the box's middle row, as uiText places them. Black
	   plane only: a cell is always over cleared background, and the second
	   message a grey clear would cost is the point of this primitive. */
	gPrintBoxText(&r, (UINT)((h >> 1) + (fontAscent[font] >> 1)), boxAlign, 0, s, (UINT)(len < 0 ? 0 : len));
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
	gSetGC0(cur->gc);
	gCopyBit(&dst, mapBitmap, &src, G_TRMODE_REPL);

	/* The grey plane is the bottom half of the same bitmap. */
	src.tl.y += 160;
	src.br.y += 160;
	gSetGC0(cur->gcGrey);
	gCopyBit(&dst, mapBitmap, &src, G_TRMODE_REPL);
	gSetGC0(cur->gc);
}

/*  The window server owns the message window, so this needs no window of
    ours and sits over the game view while blitVideoMem writes underneath. */
void uiInfoMsg(const char *s)
{
	wInfoMsg(s);
}
