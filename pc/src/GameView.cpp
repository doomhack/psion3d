#include "GameView.h"

#include <QPainter>
#include <QKeyEvent>
#include <QResizeEvent>

#include <cstring>

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

QImage psion3dFrameImage(bool showGutter)
{
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

	rebuildImage();
}

void GameView::rebuildImage()
{
	const int w = m_showGutter ? HOST_W_FULL : HOST_W;

	m_img = QImage(w, HOST_H, QImage::Format_Indexed8);
	m_img.setColorCount(4);

	for(int i = 0; i < 4; ++i)
		m_img.setColor(i, kPalette[i]);

	recomputeTarget();
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
	const int w = m_showGutter ? HOST_W_FULL : HOST_W;
	const int s = m_scale > 0 ? m_scale : 3;

	return QSize(w * s, HOST_H * s);
}

void GameView::recomputeTarget()
{
	const int iw = m_img.width();
	const int ih = m_img.height();

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
	int stride = 0;
	const unsigned char *fb = hostFramebuffer(m_showGutter ? 1 : 0, &stride);

	for(int y = 0; y < HOST_H; ++y)
		std::memcpy(m_img.scanLine(y), fb + (size_t)y * stride, (size_t)stride);

	QPainter p(this);
	p.fillRect(rect(), QColor(24, 24, 24));
	p.setRenderHint(QPainter::SmoothPixmapTransform, false);
	p.drawImage(m_dst, m_img);
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
		case Qt::Key_A:                         return HOST_KEY_LEFT;
		case Qt::Key_Right:                     return HOST_KEY_RIGHT;
		case Qt::Key_D:                         return HOST_KEY_RIGHT;
		case Qt::Key_Space:                     return HOST_KEY_USE;
		case Qt::Key_E:                         return HOST_KEY_USE;
		case Qt::Key_Control:                   return HOST_KEY_FIRE;
		case Qt::Key_Return:                    return HOST_KEY_FIRE;
		case Qt::Key_1:                         return HOST_KEY_WEAPON_1;
		case Qt::Key_2:                         return HOST_KEY_WEAPON_2;
		case Qt::Key_3:                         return HOST_KEY_WEAPON_3;
		case Qt::Key_4:                         return HOST_KEY_WEAPON_4;
		default:                                return -1;
	}
}

void GameView::keyPressEvent(QKeyEvent *e)
{
	if(e->key() == Qt::Key_P && !e->isAutoRepeat())
	{
		emit pauseToggled();
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
