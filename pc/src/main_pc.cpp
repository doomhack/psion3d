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
	QCommandLineOption verboseOpt({"v", "verbose-io"},
		"Report every failed file open, including loadSprite's routine probe "
		"past the last frame of each sprite.");

	parser.addOption(assetsOpt);
	parser.addOption(mapOpt);
	parser.addOption(tickOpt);
	parser.addOption(shotOpt);
	parser.addOption(gutterOpt);
	parser.addOption(framesOpt);
	parser.addOption(verboseOpt);
	parser.process(app);

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
