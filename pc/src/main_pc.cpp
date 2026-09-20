#include <QApplication>
#include <QCommandLineParser>

#include <cstdio>

#include "MainWindow.h"
#include "GameView.h"

extern "C" {
#include "host.h"
#include "plib_pc.h"
#include "ui.h"
}

int main(int argc, char **argv)
{
	QApplication app(argc, argv);
	app.setApplicationName("psion3d");

	QCommandLineParser parser;
	parser.setApplicationDescription("psion3d, running natively for development.");
	parser.addHelpOption();

	QCommandLineOption assetsOpt({"a", "assets"},
		"Directory holding map/ and spr/. Defaults to the source tree.", "dir");
	QCommandLineOption mapOpt({"m", "map"},
		"Start playing this map straight away, skipping the menus. Without it "
		"the program opens at the main menu, as the device does.", "n", "0");
	QCommandLineOption screenOpt("screen",
		"Open at a menu screen, for screenshots: main, select, briefing or "
		"objectives (the front end, from the first mission), or pause, abort, "
		"pobjectives or map (the pause set, which need --map).", "name");
	QCommandLineOption tickOpt("tick-start",
		"Seed the 16-bit tick counter, e.g. 65520, to exercise its wraparound "
		"in the first second rather than after 34 minutes.", "ticks");
	QCommandLineOption shotOpt("screenshot",
		"Render one frame to this PNG at 1:1 and exit, without opening a "
		"window. Compare it against an emulator capture.", "file");
	QCommandLineOption gutterOpt("gutter",
		"Include backbuffer columns 240-255, which the LCD hides.");
	QCommandLineOption framesOpt("frames",
		"Advance this many game ticks before screenshotting, one per frame. "
		"Uses the virtual clock, so the result is deterministic.", "n", "0");
	QCommandLineOption fireOpt("fire",
		"Hold the fire key down through the --frames loop, so a shot can be "
		"resolved and its effect on the map seen without opening a window.");
	QCommandLineOption useOpt("use",
		"Hold the use key down through the --frames loop, so a switch in reach "
		"is thrown and its effect on the map is visible in a headless "
		"--screenshot.");
	QCommandLineOption posOpt("pos",
		"Start the player at X,Y instead of the map's spawn. Q8 map units "
		"(256 per cell), as shown on the HUD - so 7040,384 is cell 27.5,1.5.",
		"x,y");
	QCommandLineOption angleOpt("angle",
		"Start facing this angle. Q8 radians, as shown on the HUD: 0 looks "
		"along +X, 402 is a quarter turn, 1608 is a full circle.", "a");
	QCommandLineOption verboseOpt({"v", "verbose-io"},
		"Report every failed file open, including loadSprite's routine probe "
		"past the last frame of each sprite.");

	parser.addOption(assetsOpt);
	parser.addOption(mapOpt);
	parser.addOption(screenOpt);
	parser.addOption(tickOpt);
	parser.addOption(shotOpt);
	parser.addOption(gutterOpt);
	parser.addOption(framesOpt);
	parser.addOption(fireOpt);
	parser.addOption(useOpt);
	parser.addOption(posOpt);
	parser.addOption(angleOpt);
	parser.addOption(verboseOpt);
	parser.process(app);

	/*  Parse the placement before hostInit so a typo fails without loading
	    assets. Both are Q8, matching player.pos, so a value read off the HUD
	    reproduces the same view. */
	bool placePlayer = false;
	int posX = 0, posY = 0, angle = 0;

	if(parser.isSet(posOpt))
	{
		const QStringList xy = parser.value(posOpt).split(',');
		bool okX = false, okY = false;

		if(xy.size() == 2)
		{
			posX = xy[0].trimmed().toInt(&okX);
			posY = xy[1].trimmed().toInt(&okY);
		}

		if(!okX || !okY)
		{
			std::fprintf(stderr, "--pos wants X,Y in Q8 map units, e.g. 7040,384\n");
			return 1;
		}

		placePlayer = true;
	}

	if(parser.isSet(angleOpt))
	{
		bool ok = false;

		angle = parser.value(angleOpt).toInt(&ok);

		if(!ok)
		{
			std::fprintf(stderr, "--angle wants a Q8 radian value, e.g. 402\n");
			return 1;
		}

		placePlayer = true;
	}

	if(parser.isSet(verboseOpt))
		pcSetIoVerbose(1);

	QByteArray assets;
	const char *assetRoot = nullptr;

	if(parser.isSet(assetsOpt))
	{
		assets = parser.value(assetsOpt).toLocal8Bit();
		assetRoot = assets.constData();
	}
	else if(qEnvironmentVariableIsSet("PSION3D_ASSETS"))
	{
		assets = qgetenv("PSION3D_ASSETS");
		assetRoot = assets.constData();
	}

	if(parser.isSet(tickOpt))
		pcTickSetStart((unsigned short)parser.value(tickOpt).toUInt());

	const int mapId = parser.value(mapOpt).toInt();

	/*  --screen is a key sequence played into the menus from where hostInit
	    leaves them: the main menu, or the game when --map is given. */
	struct ScreenRoute { const char *name; bool inPlay; const char *keys; };

	static const ScreenRoute routes[] =
	{
		{ "main",        false, ""     },
		{ "select",      false, "e"    },
		{ "briefing",    false, "ee"   },
		{ "objectives",  false, "eee"  },
		{ "pause",       true,  "x"    },
		{ "abort",       true,  "xddde" },
		{ "pobjectives", true,  "xde"  },
		{ "map",         true,  "xdde" }
	};

	const ScreenRoute *route = nullptr;

	if(parser.isSet(screenOpt))
	{
		const QString name = parser.value(screenOpt);

		for(const auto &r : routes)
			if(name == r.name)
				route = &r;

		if(!route)
		{
			std::fprintf(stderr, "--screen: unknown screen %s\n", qPrintable(name));
			return 1;
		}

		if(route->inPlay != (mapId != 0))
		{
			std::fprintf(stderr, "--screen %s %s --map\n", route->name,
			             route->inPlay ? "needs" : "does not take");
			return 1;
		}
	}

	if(!hostInit(assetRoot, mapId))
	{
		std::fprintf(stderr, "hostInit failed.\n");
		return 1;
	}

	if(placePlayer)
	{
		/* Either option alone keeps the spawn's value for the other. */
		HostStats spawn;

		hostGetStats(&spawn);

		if(!parser.isSet(posOpt))
		{
			posX = spawn.playerX;
			posY = spawn.playerY;
		}

		if(!parser.isSet(angleOpt))
			angle = spawn.playerAngle;

		hostSetPlayerPosition((short)posX, (short)posY, (short)angle);
	}

	if(parser.isSet(fireOpt))
		hostSetKey(HOST_KEY_FIRE, 1);

	if(parser.isSet(useOpt))
		hostSetKey(HOST_KEY_USE, 1);

	/*  Drive the virtual clock rather than waiting on the real one, so a given
	    tick count always produces the same frame. */
	for(int i = 0, n = parser.value(framesOpt).toInt(); i < n; ++i)
	{
		pcTickAdvance(1);
		hostFrame();
	}

	/*  After the frames, so a pause screen can show what play changed: --use
	    or --fire held through --frames and then --screen pobjectives shows
	    the objective it moved. With no --frames this is straight from
	    hostInit, as before. */
	if(route)
	{
		for(const char *k = route->keys; *k; ++k)
		{
			int uk = UI_KEY_NONE;

			switch(*k)
			{
				case 'e': uk = UI_KEY_ENTER; break;
				case 'x': uk = UI_KEY_ESC; break;
				case 'd': uk = UI_KEY_DOWN; break;
				case 'u': uk = UI_KEY_UP; break;
			}

			hostMenuKey(uk);
		}

		if(hostMode() != HOST_MODE_MENU)
		{
			std::fprintf(stderr, "--screen %s: the menu did not open (no missions?)\n", route->name);
			return 1;
		}
	}

	if(parser.isSet(shotOpt))
	{
		/* hostInit and the loop above have left a frame drawn. */
		const QString path = parser.value(shotOpt);

		if(!psion3dFrameImage(parser.isSet(gutterOpt)).save(path, "PNG"))
		{
			std::fprintf(stderr, "could not write %s\n", qPrintable(path));
			hostShutdown();
			return 1;
		}

		std::printf("wrote %s\n", qPrintable(path));
		hostShutdown();
		return 0;
	}

	MainWindow w;
	w.resize(w.sizeHint());
	w.show();

	const int rc = app.exec();

	hostShutdown();

	return rc;
}
