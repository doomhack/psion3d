#include "GameView.h"
#include "menu_pc.h"

#include <QPainter>
#include <QKeyEvent>
#include <QResizeEvent>
#include <QCoreApplication>

#include <cstring>

extern "C" {
#include "ui.h"
}

/*  The two planes are independent bitmasks over the same pixel, indexed here
    as (black << 1) | grey. The black plane sits on top of the grey one, so a
    pixel set in both reads as black and the display shows three shades, not
    four:

      0  neither set  - LCD background
      1  grey only    - grey
      2  black only   - black
      3  both set     - black, because black wins

    Index 3 is unreachable from the sprite blitter, which writes the planes
    mutually exclusively, but bmFillRect and bmFillPattern can produce it by
    covering both. */
#define LCD_BACKGROUND qRgb(0xC7, 0xCB, 0xA8)
#define LCD_GREY       qRgb(0x8A, 0x92, 0x74)
#define LCD_BLACK      qRgb(0x3C, 0x42, 0x30)

static const QRgb kPalette[4] =
{
	LCD_BACKGROUND,
	LCD_GREY,
	LCD_BLACK,
	LCD_BLACK
};

QImage psion3dScreenImage(bool showGutter)
{
	if(hostMode() == HOST_MODE_MENU)
		return uiPcImage().copy();

	/* The HUD panels with the game view over the middle 240 columns - the
	   whole LCD as the device shows it. With the gutter the view is 256 wide
	   and the hidden 16 columns cover the start of the right panel, which
	   is what makes them visible. */
	QImage out = uiPcHudImage().copy();
	int stride = 0;
	const unsigned char *fb = hostFramebuffer(showGutter ? 1 : 0, &stride);

	for(int y = 0; y < HOST_H; ++y)
	{
		QRgb *row = reinterpret_cast<QRgb *>(out.scanLine(y));
		const unsigned char *src = fb + (size_t)y * stride;

		for(int x = 0; x < stride && HOST_HUD_X + x < out.width(); ++x)
			row[HOST_HUD_X + x] = kPalette[src[x] & 3];
	}

	return out;
}

QImage psion3dFrameImage(bool showGutter)
{
	if(hostMode() == HOST_MODE_MENU)
		return uiPcImage().copy();

	int stride = 0;
	const unsigned char *fb = hostFramebuffer(showGutter ? 1 : 0, &stride);

	QImage img(stride, HOST_H, QImage::Format_Indexed8);
	img.setColorCount(4);

	for(int i = 0; i < 4; ++i)
		img.setColor(i, kPalette[i]);

	/* Never assume bytesPerLine() == width(); QImage rows are padded. */
	for(int y = 0; y < HOST_H; ++y)
		std::memcpy(img.scanLine(y), fb + (size_t)y * stride, (size_t)stride);

	return img;
}

GameView::GameView(QWidget *parent)
	: QWidget(parent)
{
	setFocusPolicy(Qt::StrongFocus);
	setAutoFillBackground(false);
	setMinimumSize(HOST_W, HOST_H);

	m_mode = hostMode();
	rebuildImage();
}

int GameView::imageWidth() const
{
	/* The whole LCD in both modes: the menu window, or the HUD panels with
	   the game view between them. */
	return UI_W;
}

void GameView::rebuildImage()
{
	recomputeTarget();
}

void GameView::syncMode()
{
	const int mode = hostMode();

	if(mode == m_mode)
		return;

	m_mode = mode;
	rebuildImage();
	updateGeometry();
	update();

	/* Queued: syncMode also runs from paintEvent, and the window resizing
	   itself in the middle of a paint is not allowed. */
	QMetaObject::invokeMethod(this, [this]() { emit modeChanged(); }, Qt::QueuedConnection);
}

void GameView::setScale(int scale)
{
	m_scale = scale;
	recomputeTarget();
	updateGeometry();
	update();
}

void GameView::setShowGutter(bool show)
{
	if(show == m_showGutter)
		return;

	m_showGutter = show;
	rebuildImage();
	updateGeometry();
	update();
}

QSize GameView::sizeHint() const
{
	const int s = m_scale > 0 ? m_scale : 3;

	return QSize(imageWidth() * s, HOST_H * s);
}

void GameView::recomputeTarget()
{
	const int iw = imageWidth();
	const int ih = HOST_H;

	int s = m_scale;

	if(s <= 0)
	{
		/* Fit, but only at whole multiples: fractional scaling of a 240x160
		   image makes the pixel grid uneven and the dither patterns lie. */
		s = qMax(1, qMin(width() / iw, height() / ih));
	}

	m_dst = QRect((width() - iw * s) / 2, (height() - ih * s) / 2, iw * s, ih * s);
}

void GameView::resizeEvent(QResizeEvent *)
{
	recomputeTarget();
}

void GameView::paintEvent(QPaintEvent *)
{
	syncMode();

	QPainter p(this);
	p.fillRect(rect(), QColor(24, 24, 24));
	p.setRenderHint(QPainter::SmoothPixmapTransform, false);

	p.drawImage(m_dst, psion3dScreenImage(m_showGutter));
}

int GameView::hostKeyFor(int qtKey)
{
	switch(qtKey)
	{
		case Qt::Key_Up:                        return HOST_KEY_UP;
		case Qt::Key_W:                         return HOST_KEY_UP;
		case Qt::Key_Down:                      return HOST_KEY_DOWN;
		case Qt::Key_S:                         return HOST_KEY_DOWN;
		case Qt::Key_Left:                      return HOST_KEY_LEFT;
		case Qt::Key_Right:                     return HOST_KEY_RIGHT;
		case Qt::Key_A:                         return HOST_KEY_STRAFE_LEFT;
		case Qt::Key_D:                         return HOST_KEY_STRAFE_RIGHT;
		case Qt::Key_Space:                     return HOST_KEY_USE;
		case Qt::Key_E:                         return HOST_KEY_USE;
		case Qt::Key_Control:                   return HOST_KEY_FIRE;
		case Qt::Key_Return:                    return HOST_KEY_FIRE;
		case Qt::Key_1:                         return HOST_KEY_WEAPON_1;
		case Qt::Key_2:                         return HOST_KEY_WEAPON_2;
		case Qt::Key_3:                         return HOST_KEY_WEAPON_3;
		case Qt::Key_4:                         return HOST_KEY_WEAPON_4;
		case Qt::Key_Comma:                     return HOST_KEY_STRAFE_LEFT;
		case Qt::Key_Period:                    return HOST_KEY_STRAFE_RIGHT;
		default:                                return -1;
	}
}

/*  The menu keys, the same set the device reads from its key events. */
int GameView::uiKeyFor(int qtKey)
{
	switch(qtKey)
	{
		case Qt::Key_Up:                        return UI_KEY_UP;
		case Qt::Key_Down:                      return UI_KEY_DOWN;
		case Qt::Key_Left:                      return UI_KEY_LEFT;
		case Qt::Key_Right:                     return UI_KEY_RIGHT;
		case Qt::Key_Return:                    return UI_KEY_ENTER;
		case Qt::Key_Enter:                     return UI_KEY_ENTER;
		case Qt::Key_Escape:                    return UI_KEY_ESC;
		case Qt::Key_Space:                     return UI_KEY_SPACE;
		default:                                return UI_KEY_NONE;
	}
}

void GameView::keyPressEvent(QKeyEvent *e)
{
	if(e->key() == Qt::Key_P && !e->isAutoRepeat())
	{
		emit pauseToggled();
		return;
	}

	/*  Menus take every press, auto-repeat included, so a held arrow scrolls
	    a list; in play only Esc is an event, and it opens the pause menu. */
	const int uk = uiKeyFor(e->key());

	if(uk != UI_KEY_NONE && (hostMode() == HOST_MODE_MENU || uk == UI_KEY_ESC))
	{
		const int r = hostMenuKey(uk);

		if(r == HOST_MENU_QUIT)
		{
			QCoreApplication::quit();
			return;
		}

		if(r == HOST_MENU_REPAINT)
		{
			syncMode();
			update();
		}

		return;
	}

	/* Auto-repeat would produce a press/release stream for a held key, which
	   makes movement stutter. The device reports a level, not an edge. */
	if(e->isAutoRepeat())
		return;

	const int k = hostKeyFor(e->key());

	if(k < 0)
	{
		QWidget::keyPressEvent(e);
		return;
	}

	hostSetKey((HostKey)k, 1);
}

void GameView::keyReleaseEvent(QKeyEvent *e)
{
	if(e->isAutoRepeat())
		return;

	const int k = hostKeyFor(e->key());

	if(k < 0)
	{
		QWidget::keyReleaseEvent(e);
		return;
	}

	hostSetKey((HostKey)k, 0);
}

void GameView::focusOutEvent(QFocusEvent *e)
{
	/* Mirrors the device's WM_BACKGROUND handling: drop every key so nothing
	   is stuck down when focus comes back. */
	hostClearKeys();
	QWidget::focusOutEvent(e);
}
