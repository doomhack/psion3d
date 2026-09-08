#include "MainWindow.h"
#include "GameView.h"

#include <QLabel>
#include <QTimer>
#include <QWidget>
#include <QHBoxLayout>
#include <QMenuBar>
#include <QActionGroup>
#include <QFontDatabase>

extern "C" {
#include "host.h"
}

MainWindow::MainWindow(QWidget *parent)
	: QMainWindow(parent)
{
	setWindowTitle("psion3d (PC)");

	m_view = new GameView(this);
	connect(m_view, &GameView::pauseToggled, this, &MainWindow::togglePause);

	/*  The device draws fps, position, health, angle and the debug slot into a
	    separate 120x160 window through gPrintText. Same content, but as a real
	    HUD with room to grow. */
	m_hud = new QLabel(this);
	m_hud->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
	m_hud->setAlignment(Qt::AlignTop | Qt::AlignLeft);
	m_hud->setMinimumWidth(190);
	m_hud->setTextInteractionFlags(Qt::TextSelectableByMouse);

	QWidget *central = new QWidget(this);
	QHBoxLayout *layout = new QHBoxLayout(central);
	layout->setContentsMargins(0, 0, 8, 0);
	layout->addWidget(m_view, 1);
	layout->addWidget(m_hud, 0);
	setCentralWidget(central);

	buildMenus();

	m_frameTimer = new QTimer(this);
	connect(m_frameTimer, &QTimer::timeout, this, &MainWindow::onFrame);

	m_hudTimer = new QTimer(this);
	connect(m_hudTimer, &QTimer::timeout, this, &MainWindow::refreshHud);
	m_hudTimer->start(250);

	m_fpsClock.start();
	applyPacing();
	refreshHud();

	m_view->setFocus();
}

void MainWindow::buildMenus()
{
	QMenu *viewMenu = menuBar()->addMenu("&View");

	QActionGroup *scaleGroup = new QActionGroup(this);

	for(int s = 1; s <= 4; ++s)
	{
		QAction *a = viewMenu->addAction(QString("%1x").arg(s));
		a->setCheckable(true);
		a->setChecked(s == m_view->scale());
		a->setShortcut(QKeySequence(Qt::CTRL | (Qt::Key)(Qt::Key_1 + s - 1)));
		scaleGroup->addAction(a);

		connect(a, &QAction::triggered, this, [this, s]()
		{
			m_view->setScale(s);
			resize(sizeHint());
		});
	}

	QAction *fit = viewMenu->addAction("Fit window");
	fit->setCheckable(true);
	scaleGroup->addAction(fit);
	connect(fit, &QAction::triggered, this, [this]() { m_view->setScale(0); });

	viewMenu->addSeparator();

	QAction *gutter = viewMenu->addAction("Show hidden columns 240-255");
	gutter->setCheckable(true);
	gutter->setToolTip("The backbuffer is 256 wide but the LCD shows 240. "
	                   "Anything drawn out here is a clipping bug.");
	connect(gutter, &QAction::toggled, this, [this](bool on)
	{
		m_view->setShowGutter(on);
		resize(sizeHint());
	});

	QMenu *runMenu = menuBar()->addMenu("&Run");

	QAction *pause = runMenu->addAction("Pause");
	pause->setCheckable(true);
	pause->setShortcut(Qt::Key_P);
	connect(pause, &QAction::toggled, this, [this](bool on)
	{
		m_paused = on;
		hostSetPaused(on ? 1 : 0);

		if(!on)
			hostResyncClock();
	});

	runMenu->addSeparator();

	QActionGroup *paceGroup = new QActionGroup(this);

	struct { const char *name; Pacing mode; int fps; } modes[] =
	{
		{ "Device pacing (20 fps)", PaceDevice, 20 },
		{ "Device pacing (10 fps)", PaceDevice, 10 },
		{ "Vsync (60 fps)",         PaceVsync,  60 },
		{ "Free running",           PaceFree,    0 }
	};

	for(const auto &m : modes)
	{
		QAction *a = runMenu->addAction(m.name);
		a->setCheckable(true);
		a->setChecked(m.mode == m_pacing && (m.fps == m_targetFps || m.fps == 0));
		paceGroup->addAction(a);

		connect(a, &QAction::triggered, this, [this, m]()
		{
			m_pacing = m.mode;

			if(m.fps)
				m_targetFps = m.fps;

			applyPacing();
		});
	}
}

void MainWindow::applyPacing()
{
	int interval = 0;

	switch(m_pacing)
	{
		case PaceDevice: interval = 1000 / (m_targetFps > 0 ? m_targetFps : 20); break;
		case PaceVsync:  interval = 16; break;
		case PaceFree:   interval = 0; break;
	}

	m_frameTimer->start(interval);
}

void MainWindow::togglePause()
{
	m_paused = !m_paused;
	hostSetPaused(m_paused ? 1 : 0);

	if(!m_paused)
		hostResyncClock();
}

void MainWindow::onFrame()
{
	if(!m_paused)
		hostFrame();

	m_view->update();

	++m_frames;

	if(m_fpsClock.elapsed() >= 1000)
	{
		m_fps = (int)(m_frames * 1000LL / m_fpsClock.elapsed());
		m_frames = 0;
		m_fpsClock.restart();
	}
}

void MainWindow::refreshHud()
{
	HostStats s;
	hostGetStats(&s);

	const char *paceName = m_pacing == PaceDevice ? "device"
	                     : m_pacing == PaceVsync  ? "vsync"
	                                              : "free";

	/*  Labelled "PC fps" on purpose. It is not comparable to the device's
	    figure and must never be used to judge a renderer change - measure that
	    on hardware, per CLAUDE.md. */
	m_hud->setText(QString(
		"PC fps   %1%2\n"
		"pacing   %3\n"
		"ticks/fr %4\n"
		"\n"
		"pos      %5, %6\n"
		"angle    %7\n"
		"health   %8\n"
		"gameTime %9\n"
		"\n"
		"dbg      %10\n"
		"\n"
		"arrows/WASD move\n"
		"space fire\n"
		"1-4 weapons\n"
		"P pause")
		.arg(m_fps)
		.arg(m_paused ? "  (paused)" : "")
		.arg(paceName)
		.arg(s.ticksLastFrame)
		.arg(s.playerX).arg(s.playerY)
		.arg(s.playerAngle)
		.arg(s.health)
		.arg(s.gameTime)
		.arg(hostDebugText()));
}
