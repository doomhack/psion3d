#include <QApplication>
#include <QCommandLineParser>

#include <cstdio>

#include "MainWindow.h"
#include "GameView.h"

extern "C" {
#include "host.h"
#include "plib_pc.h"
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
	QCommandLineOption mapOpt({"m", "map"}, "Map to load. Default 1.", "n", "1");
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

	if(!hostInit(assetRoot, parser.value(mapOpt).toInt()))
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
