/*  menu_pc.cpp - the PC side of ui.h: the menu screens drawn with QPainter.

    The device draws these with WLIB into a 480x160 window using the ROM
    Swiss 13 and Swiss 16 fonts. Here the same primitives paint a QImage with
    the LCD palette and Helvetica at the same pixel sizes - close enough to
    check layout and logic, not a pixel match for the device. Golden frames
    of the menus are therefore frames of THIS renderer.

    ui.h has no includes and plain C types for exactly this file: nothing
    here may pull in a game header (see host.h). */

extern "C" {
#include "ui.h"
#include "host.h"
}

#include "menu_pc.h"

#include <QImage>
#include <QPainter>
#include <QFont>
#include <QFontMetrics>
#include <QString>

#include <cstring>
#include <cstdio>

#define LCD_BACKGROUND qRgb(0xC7, 0xCB, 0xA8)
#define LCD_GREY       qRgb(0x8A, 0x92, 0x74)
#define LCD_BLACK      qRgb(0x3C, 0x42, 0x30)

/*  The two planes are kept apart, as on the device, and composed on demand:
    the black plane sits over the grey one, so both set reads as black. One
    pair per target - the menu window and the HUD window - with references
    that follow uiTarget, so the primitives below never care which. */
struct Planes
{
	QImage black;
	QImage grey;
	QImage composed;
};

static Planes g_planes[2];
static int    g_target = UI_TARGET_MENU;
static bool   g_ready = false;

#define g_black (g_planes[g_target].black)
#define g_grey (g_planes[g_target].grey)

static QFont fontFor(int font)
{
	QFont f("Helvetica");

	f.setStyleStrategy(QFont::NoAntialias);
	f.setPixelSize((font == UI_FONT_HEAD_BOLD || font == UI_FONT_BIG) ? 16 : 13);
	f.setBold(font != UI_FONT_BODY);

	/* The ROM Swiss fonts run about a tenth wider than Helvetica at the
	   same height (measured off device grabs), and a wrap or an ellipsis
	   that only shows on the device is exactly what this build is meant to
	   catch. */
	f.setStretch(110);

	return f;
}

static void ensure()
{
	if(g_ready)
		return;

	for(Planes &p : g_planes)
	{
		p.black = QImage(UI_W, UI_H, QImage::Format_Grayscale8);
		p.grey = QImage(UI_W, UI_H, QImage::Format_Grayscale8);
		p.composed = QImage(UI_W, UI_H, QImage::Format_RGB32);
		p.black.fill(0);
		p.grey.fill(0);
	}

	g_ready = true;
}

/*  Text arrives as code page 850 bytes from the game side; the one
    non-ASCII byte the menus use is the middle dot. */
static QString decode(const char *s, int len)
{
	QString out;

	out.reserve(len);

	for(int i = 0; i < len; ++i)
	{
		const unsigned char c = (unsigned char)s[i];

		if(c == 0xFA)
			out.append(QChar(0x00B7));
		else
			out.append(QChar(c));
	}

	return out;
}

static void fillPlane(QImage &plane, short x, short y, short w, short h, int value)
{
	QPainter p(&plane);
	p.fillRect(x, y, w, h, QColor(value, value, value));
}

static const QImage &compose(Planes &p)
{
	ensure();

	for(int y = 0; y < UI_H; ++y)
	{
		const uchar *b = p.black.constScanLine(y);
		const uchar *g = p.grey.constScanLine(y);
		QRgb *out = reinterpret_cast<QRgb *>(p.composed.scanLine(y));

		for(int x = 0; x < UI_W; ++x)
			out[x] = b[x] ? LCD_BLACK : (g[x] ? LCD_GREY : LCD_BACKGROUND);
	}

	return p.composed;
}

const QImage &uiPcImage()
{
	return compose(g_planes[UI_TARGET_MENU]);
}

const QImage &uiPcHudImage()
{
	return compose(g_planes[UI_TARGET_HUD]);
}

extern "C" {

void uiTarget(short target)
{
	g_target = (target == UI_TARGET_HUD) ? UI_TARGET_HUD : UI_TARGET_MENU;
}

void uiClear(void)
{
	ensure();
	g_black.fill(0);
	g_grey.fill(0);
}

void uiFillRect(short x, short y, short w, short h)
{
	ensure();
	fillPlane(g_black, x, y, w, h, 255);
}

void uiClearRect(short x, short y, short w, short h)
{
	ensure();
	fillPlane(g_black, x, y, w, h, 0);
	fillPlane(g_grey, x, y, w, h, 0);
}

void uiGreyRect(short x, short y, short w, short h)
{
	ensure();
	fillPlane(g_grey, x, y, w, h, 255);
}

void uiPatternRect(short x, short y, short w, short h)
{
	ensure();

	/* The system dither: alternate pixels, offset every other row, on the
	   black plane, replacing whatever was there. */
	for(int yy = y; yy < y + h; ++yy)
	{
		if(yy < 0 || yy >= UI_H)
			continue;

		uchar *row = g_black.scanLine(yy);

		for(int xx = x; xx < x + w; ++xx)
		{
			if(xx < 0 || xx >= UI_W)
				continue;

			row[xx] = ((xx + yy) & 1) ? 255 : 0;
		}
	}
}

void uiBox(short x, short y, short w, short h)
{
	ensure();

	if(w <= 0 || h <= 0)
		return;

	fillPlane(g_black, x, y, w, 1, 255);
	fillPlane(g_black, x, (short)(y + h - 1), w, 1, 255);
	fillPlane(g_black, x, y, 1, h, 255);
	fillPlane(g_black, (short)(x + w - 1), y, 1, h, 255);
}

void uiLine(short x0, short y0, short x1, short y1)
{
	ensure();

	QPainter p(&g_black);
	p.setPen(QColor(255, 255, 255));
	p.drawLine(x0, y0, x1, y1);
	p.drawPoint(x1, y1);
}

void uiText(short x, short y, short font, short inverse, const char *s, short len)
{
	ensure();

	if(len <= 0)
		return;

	const QFont f = fontFor(font);
	const QFontMetrics fm(f);
	const QString str = decode(s, len);

	/* Text is a mask: paint white glyphs onto a scratch plane, then set or
	   clear the black plane where they landed. Antialiasing is off so the
	   mask is crisp, as the device's bitmap fonts are. */
	QImage mask(UI_W, UI_H, QImage::Format_Grayscale8);
	mask.fill(0);

	{
		QPainter p(&mask);
		p.setRenderHint(QPainter::TextAntialiasing, false);
		p.setFont(f);
		p.setPen(QColor(255, 255, 255));
		/* y is the row centre; centre the capitals on it as the device does.
		   The big face is the head face with every row doubled, so its mask
		   is painted at half scale about the same centre and stretched below. */
		const int capHalf = (font == UI_FONT_BIG) ? fm.capHeight() : fm.capHeight() / 2;
		const int baseline = (font == UI_FONT_BIG) ? (y / 2 + capHalf / 2) : (y + capHalf);
		p.drawText(x, baseline, str);
	}

	for(int yy = 0; yy < UI_H; ++yy)
	{
		/* Doubled rows: screen row yy reads mask row yy / 2, positioned so the
		   caps straddle y as the device's G_STY_DOUBLE output does. */
		const int my = (font == UI_FONT_BIG) ? (yy / 2) : yy;
		const uchar *m = mask.constScanLine(my);
		uchar *b = g_black.scanLine(yy);

		for(int xx = 0; xx < UI_W; ++xx)
		{
			if(m[xx] >= 128)
				b[xx] = inverse ? 0 : 255;
		}
	}
}

short uiTextWidth(short font, const char *s, short len)
{
	if(len <= 0)
		return 0;

	const QFontMetrics fm(fontFor(font));

	return (short)fm.horizontalAdvance(decode(s, len));
}

void uiTextBox(short x, short y, short w, short h, short font, short inverse, short align, const char *s, short len)
{
	ensure();

	if(w <= 0 || h <= 0)
		return;

	/* The device replaces the whole box in one call; here it is the fill and
	   the text, clipped to the box, the text placed by its measured width. */
	fillPlane(g_black, x, y, w, h, inverse ? 255 : 0);

	if(len <= 0)
		return;

	const short tw = uiTextWidth(font, s, len);
	short tx = x;

	if(align == UI_ALIGN_RIGHT)
		tx = (short)(x + w - tw);
	else if(align == UI_ALIGN_CENTRE)
		tx = (short)(x + ((w - tw) >> 1));

	/* Clip by drawing through a copy of the plane and taking only the box. */
	QImage before = g_black.copy();
	uiText(tx, (short)(y + (h >> 1)), font, inverse, s, len);

	for(int yy = 0; yy < UI_H; ++yy)
	{
		if(yy >= y && yy < y + h)
			continue;

		std::memcpy(g_black.scanLine(yy), before.constScanLine(yy), UI_W);
	}

	for(int yy = y; yy < y + h && yy < UI_H; ++yy)
	{
		if(yy < 0)
			continue;

		uchar *row = g_black.scanLine(yy);
		const uchar *old = before.constScanLine(yy);

		for(int xx = 0; xx < UI_W; ++xx)
			if(xx < x || xx >= x + w)
				row[xx] = old[xx];
	}
}

void uiBlitMap(short dstX, short dstY, short srcY, short w, short h)
{
	ensure();

	int stride = 0;
	const unsigned char *fb = hostFramebuffer(1, &stride);

	for(int yy = 0; yy < h; ++yy)
	{
		const int sy = srcY + yy;
		const int dy = dstY + yy;

		if(sy < 0 || sy >= HOST_H || dy < 0 || dy >= UI_H)
			continue;

		const unsigned char *src = fb + (size_t)sy * stride;
		uchar *b = g_black.scanLine(dy);
		uchar *g = g_grey.scanLine(dy);

		for(int xx = 0; xx < w && xx < stride; ++xx)
		{
			const int dx = dstX + xx;

			if(dx < 0 || dx >= UI_W)
				continue;

			/* hostFramebuffer packs (black << 1) | grey. */
			b[dx] = (src[xx] & 2) ? 255 : 0;
			g[dx] = (src[xx] & 1) ? 255 : 0;
		}
	}
}

/*  The device pops the window server's info message; the PC has no such
    thing, and a line on stderr is what a headless run can see. */
void uiInfoMsg(const char *s)
{
	std::fprintf(stderr, "info: %s\n", s);
}

} /* extern "C" */
