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

#define LCD_BACKGROUND qRgb(0xC7, 0xCB, 0xA8)
#define LCD_GREY       qRgb(0x8A, 0x92, 0x74)
#define LCD_BLACK      qRgb(0x3C, 0x42, 0x30)

/*  The two planes are kept apart, as on the device, and composed on demand:
    the black plane sits over the grey one, so both set reads as black. */
static QImage g_black;
static QImage g_grey;
static QImage g_composed;
static bool   g_ready = false;

static QFont fontFor(int font)
{
	QFont f("Helvetica");

	f.setStyleStrategy(QFont::NoAntialias);
	f.setPixelSize(font == UI_FONT_HEAD_BOLD ? 16 : 13);
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

	g_black = QImage(UI_W, UI_H, QImage::Format_Grayscale8);
	g_grey = QImage(UI_W, UI_H, QImage::Format_Grayscale8);
	g_composed = QImage(UI_W, UI_H, QImage::Format_RGB32);
	g_black.fill(0);
	g_grey.fill(0);
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

const QImage &uiPcImage()
{
	ensure();

	for(int y = 0; y < UI_H; ++y)
	{
		const uchar *b = g_black.constScanLine(y);
		const uchar *g = g_grey.constScanLine(y);
		QRgb *out = reinterpret_cast<QRgb *>(g_composed.scanLine(y));

		for(int x = 0; x < UI_W; ++x)
			out[x] = b[x] ? LCD_BLACK : (g[x] ? LCD_GREY : LCD_BACKGROUND);
	}

	return g_composed;
}

extern "C" {

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
		/* y is the row centre; centre the capitals on it as the device does. */
		p.drawText(x, y + fm.capHeight() / 2, str);
	}

	for(int yy = 0; yy < UI_H; ++yy)
	{
		const uchar *m = mask.constScanLine(yy);
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

} /* extern "C" */
