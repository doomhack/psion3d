#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QElapsedTimer>

class GameView;
class QLabel;
class QTimer;

class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	/*  Frame pacing. The device renders uncapped and lets runTicks() absorb
	    whatever rate results, which on a 27 MHz V30 is about 20fps against 32
	    ticks a second - one or two ticks per frame. Uncapped on a PC means
	    thousands of frames a second and zero ticks on nearly all of them, so
	    the catch-up loop never runs more than once and the game feels wrong.
	    Emulating the device rate is therefore the default. */
	enum Pacing { PaceDevice, PaceVsync, PaceFree };

	explicit MainWindow(QWidget *parent = nullptr);

private slots:
	void onFrame();
	void refreshHud();
	void togglePause();

private:
	void buildMenus();
	void applyPacing();

	GameView *m_view = nullptr;
	QLabel   *m_hud = nullptr;
	QTimer   *m_frameTimer = nullptr;
	QTimer   *m_hudTimer = nullptr;

	Pacing m_pacing = PaceDevice;
	int    m_targetFps = 20;
	bool   m_paused = false;

	QElapsedTimer m_fpsClock;
	int m_frames = 0;
	int m_fps = 0;
};

#endif
