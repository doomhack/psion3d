#ifndef GAMELOOP_H
#define GAMELOOP_H

#include "fp_types.h"

/*  The parts of the frame that are the same on every target.

    psion3d.c keeps everything platform-specific around this: the WLIB window
    setup and event pump, p_getscancodes, and the blit. The PC host in pc/src
    supplies its own equivalents. Both then call gameRunTicks, so there is only
    one copy of the timing and update order. */

/*  The current KEY_* bitmask, filled by whichever platform is reading input. */
extern u16 keys;

/*  The tick counter is 16 bits and free-running, so it wraps roughly every
    2048 seconds. Both helpers are written to stay correct across that wrap;
    widening either of them would break it. */
s16 tickDelta(const u16 later, const u16 earlier);
u16 tickElapsed(const u16 later, const u16 earlier);

/*  Catch the simulation up to realTime in whole ticks, then render one frame
    into the bitmap. Returns the new game time. Presenting the bitmap is the
    platform's job and is deliberately not done here. */
u16 gameRunTicks(u16 gameTime, const u16 realTime);

#endif
