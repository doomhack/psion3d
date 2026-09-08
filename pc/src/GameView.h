#ifndef GAMEVIEW_H
#define GAMEVIEW_H

#include <QWidget>
#include <QImage>

extern "C" {
#include "host.h"
}

/*  Builds a QImage of the current host framebuffer, applying the 4-entry LCD
    palette. Shared by the widget and by the --screenshot path, so there is
    only ever one copy of the plane-to-shade mapping. */
QImage psion3dFrameImage(bool showGutter);

/*  Shows the game's two-plane 1bpp backbuffer as an integer-scaled image and
    turns Qt key events into host key state. */
class GameView : public QWidget
{
	Q_OBJECT

public:
	explicit GameView(QWidget *parent = nullptr);

	void setScale(int scale);          /* 0 = fit the widget */
	int  scale() const { return m_scale; }

	void setShowGutter(bool show);
	bool showGutter() const { return m_showGutter; }

	QSize sizeHint() const override;

signals:
	void pauseToggled();

protected:
	void paintEvent(QPaintEvent *) override;
	void resizeEvent(QResizeEvent *) override;
	void keyPressEvent(QKeyEvent *) override;
	void keyReleaseEvent(QKeyEvent *) override;
	void focusOutEvent(QFocusEvent *) override;

private:
	void rebuildImage();
	void recomputeTarget();
	static int hostKeyFor(int qtKey);

	QImage m_img;
	QRect  m_dst;
	int    m_scale = 3;
	bool   m_showGutter = false;
};

#endif
