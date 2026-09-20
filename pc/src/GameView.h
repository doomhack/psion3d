#ifndef GAMEVIEW_H
#define GAMEVIEW_H

#include <QWidget>
#include <QImage>

extern "C" {
#include "host.h"
}

/*  Builds a QImage of the current host framebuffer, applying the 4-entry LCD
    palette - or of the menu, when that is what the game is showing. Shared
    by the widget and by the --screenshot path, so there is only ever one
    copy of the plane-to-shade mapping. */
QImage psion3dFrameImage(bool showGutter);

/*  The whole 480x160 LCD: the menu, or the HUD panels with the game view
    composited over the middle - what the widget shows and what --hud
    screenshots. psion3dFrameImage stays the bare game view, so the gameplay
    goldens do not move with the HUD. */
QImage psion3dScreenImage(bool showGutter);

/*  Shows the game's two-plane 1bpp backbuffer as an integer-scaled image and
    turns Qt key events into host key state. In the menu it shows the 480x160
    menu image instead and turns key presses into UI_KEY_* edges. */
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

	/*  Re-fit to the host's mode if it changed. Cheap when it did not. */
	void syncMode();

signals:
	void pauseToggled();

	/*  The image is 480 wide in the menu and 240 in play, so the window
	    re-fits itself when the mode switches. */
	void modeChanged();

protected:
	void paintEvent(QPaintEvent *) override;
	void resizeEvent(QResizeEvent *) override;
	void keyPressEvent(QKeyEvent *) override;
	void keyReleaseEvent(QKeyEvent *) override;
	void focusOutEvent(QFocusEvent *) override;

private:
	void rebuildImage();
	void recomputeTarget();
	int  imageWidth() const;
	static int hostKeyFor(int qtKey);
	static int uiKeyFor(int qtKey);

	QRect  m_dst;
	int    m_scale = 3;
	bool   m_showGutter = false;
	int    m_mode = -1;             /* HOST_MODE_* the image was built for */
};

#endif
