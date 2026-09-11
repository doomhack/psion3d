/*  host.h - the entire surface the Qt layer sees.

    Deliberately includes no game header. fp_types.h punning fpsplit_t through
    a union is legal C but formally UB in C++, and psion3d.h drags in the whole
    fixed-point layer, so no .cpp in this build may include a game header. The
    plain-C types below are the boundary. */

/* Guard is not HOST_H: that name is taken by the screen height macro below. */
#ifndef PSION3D_HOST_H
#define PSION3D_HOST_H

#ifdef __cplusplus
extern "C" {
#endif

#define HOST_W      240     /* what the LCD shows */
#define HOST_H      160
#define HOST_W_FULL 256     /* what the backbuffer actually holds, see below */

/*  Named rather than a bitmask so the KEY_* values from psion3d.h are never
    duplicated on the C++ side. host.c owns the single mapping table. */
typedef enum
{
	HOST_KEY_UP, HOST_KEY_DOWN, HOST_KEY_LEFT, HOST_KEY_RIGHT, HOST_KEY_FIRE,
	HOST_KEY_WEAPON_1, HOST_KEY_WEAPON_2, HOST_KEY_WEAPON_3, HOST_KEY_WEAPON_4,
	HOST_KEY_USE,
	HOST_KEY_COUNT
} HostKey;

typedef struct
{
	unsigned short gameTime;
	unsigned short ticksLastFrame;   /* catch-up ticks the last frame ran */
	short          playerX, playerY, playerAngle;
	unsigned char  health;
} HostStats;

/*  assetRoot may be NULL to keep the compiled-in default. Returns 0 on
    failure, having already printed why. */
int  hostInit(const char *assetRoot, int mapId);
void hostShutdown(void);

void hostSetKey(HostKey k, int down);
void hostClearKeys(void);           /* on focus loss, so nothing sticks down */

/*  One frame: catch up whole ticks against the tick counter, then clear,
    draw and unpack. */
void hostFrame(void);

/*  8-bit indexed pixels, one byte per pixel, HOST_H rows.

    showGutter exposes the 16 columns the device renders but the LCD hides:
    the backbuffer is 256 wide because 32-byte rows make addressing a shift,
    and blitVideoMem copies all 32 bytes into a 60-byte-stride framebuffer, so
    columns 240..255 are written and never seen. Showing them turns clipping
    bugs into something visible. *outWidth receives the row stride. */
const unsigned char *hostFramebuffer(int showGutter, int *outWidth);

const char *hostDebugText(void);
void        hostGetStats(HostStats *out);

/*  Pausing has to stop the tick source too, or the first frame after resume
    runs hundreds of catch-up ticks. */
void hostSetPaused(int paused);
void hostResyncClock(void);

#ifdef __cplusplus
}
#endif

#endif
