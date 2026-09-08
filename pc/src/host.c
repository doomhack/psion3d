/*  host.c - drives the game from the Qt side.

    This is the only PC file that includes game headers, and it is C, not C++,
    for the reason given in host.h. */

#include <plib.h>	/* p_returntickcount; psion3d.h no longer supplies it. */

#include "host.h"
#include "plib_pc.h"
#include "fpasm_pc.h"
#include "debug_pc.h"

#include "psion3d.h"
#include "gameloop.h"
#include "bitmap.h"
#include "draw.h"
#include "game_map.h"
#include "player.h"
#include "enemy.h"
#include "units.h"

#include <stdio.h>
#include <string.h>

/*  HostKey -> KEY_*. The device reaches the same bitmask by reading a raw
    10-word hardware key matrix with p_getscancodes and testing nine bits
    (psion3d.c:49-83). The matrix is transport, not behaviour: `keys` is the
    real seam, and it is what updatePlayer() consumes. */
static const u16 g_keyBits[HOST_KEY_COUNT] =
{
	KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_FIRE,
	KEY_WEAPON_1, KEY_WEAPON_2, KEY_WEAPON_3, KEY_WEAPON_4
};

static u16 g_gameTime = 0;
static u16 g_lastTicks = 0;
static int g_started = 0;

static unsigned char g_fb[HOST_H * HOST_W_FULL];
static int           g_fbWidth = HOST_W;

/* ------------------------------------------------------------------- init */

int hostInit(const char *assetRoot, int mapId)
{
	if(!fpLayoutOk())
	{
		fprintf(stderr,
		        "fpsplit_t does not overlay s32 on this host: fpdiv would return\n"
		        "garbage. s32 is `long`, which is 64 bits on LP64.\n");
		return 0;
	}

	pcSetAssetRoot(assetRoot);

	if(!loadMap((u8)mapId))
	{
		fprintf(stderr, "loadMap(%d) failed. Asset root is %s\n", mapId, pcGetAssetRoot());
		return 0;
	}

	initPlayer();

	pcTickResync();
	g_gameTime = p_returntickcount();
	g_started = 1;

	/* Render one frame immediately so the window has something to show before
	   the pacing timer has fired. */
	bmClearScreen();
	draw();

	return 1;
}

void hostShutdown(void)
{
	pcSegReleaseAll();
	g_started = 0;
}

/* ------------------------------------------------------------------ input */

void hostSetKey(HostKey k, int down)
{
	if(k < 0 || k >= HOST_KEY_COUNT)
		return;

	if(down)
		keys |= g_keyBits[k];
	else
		keys &= (u16)~g_keyBits[k];
}

void hostClearKeys(void)
{
	keys = 0;
}

/* ------------------------------------------------------------------ frame */

/*  The mirror of psion3d.c's runTicks(): sample the clock, run the shared
    loop, and let the caller present. There is no second copy of the catch-up
    logic - gameRunTicks is the same object code the device runs. */
void hostFrame(void)
{
	u16 before;

	if(!g_started)
		return;

	before = g_gameTime;

	g_gameTime = gameRunTicks(g_gameTime, p_returntickcount());

	g_lastTicks = (u16)(g_gameTime - before);
}

void hostSetPaused(int paused)
{
	pcTickSetPaused(paused);
}

void hostResyncClock(void)
{
	pcTickResync();
	g_gameTime = p_returntickcount();
}

/* ------------------------------------------------------------ framebuffer */

/*  Two 1bpp planes over the same pixel, both inside the single screenBm
    buffer: blackBm at the start, greyBm BM_WORDS later. Rows are 32 bytes, so
    the row offset is y << 5, and pixels are LOW BIT FIRST - pixel x lives in
    bit (x & 7). Reversing that bit order swaps column pairs and shows up as
    jagged wall edges. */
const unsigned char *hostFramebuffer(int showGutter, int *outWidth)
{
	const u16 width = (u16)(showGutter ? HOST_W_FULL : HOST_W);
	const u16 bytesPerRow = (u16)(width >> 3);      /* 32 or 30 */
	u16 y;

	g_fbWidth = width;

	for(y = 0; y < HOST_H; y++)
	{
		const u8 *blkRow = blackBm + ((u16)y << 5);
		const u8 *gryRow = greyBm  + ((u16)y << 5);
		unsigned char *out = &g_fb[(unsigned)y * width];
		u16 bx;

		for(bx = 0; bx < bytesPerRow; bx++)
		{
			const u8 blk = blkRow[bx];
			const u8 gry = gryRow[bx];
			u8 bit;

			for(bit = 0; bit < 8; bit++)
				*out++ = (unsigned char)((((blk >> bit) & 1) << 1) | ((gry >> bit) & 1));
		}
	}

	if(outWidth)
		*outWidth = g_fbWidth;

	return g_fb;
}

const char *hostDebugText(void)
{
	return pcDebugText();
}

void hostGetStats(HostStats *out)
{
	if(!out)
		return;

	out->gameTime       = g_gameTime;
	out->ticksLastFrame = g_lastTicks;
	out->playerX        = (short)player.pos.x;
	out->playerY        = (short)player.pos.y;
	out->playerAngle    = (short)player.pos.angle;
	out->health         = player.health;
}
