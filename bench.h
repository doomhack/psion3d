#ifndef BENCH_H
#define BENCH_H

#include "fp_types.h"
#include "game_map.h"
#include "units.h"	/* TICKS_PER_SECOND */

/*  The benchmark: the stations of map97.map in turn, each rendered from a
    fixed position for BENCH_STATION_TICKS with input ignored and the world
    frozen (no player, AI or mission ticks), and a frame rate per station in
    tenths for the results screen. Started from the Options screen; the run
    ends on MENU_BENCH the way a mission ends on its outcome, so the
    platforms need nothing new. Portable: gameloop.c calls it on both. */

#define BENCH_MAP_ID 97
#define BENCH_STATION_TICKS (5 * TICKS_PER_SECOND)

/*  A run is in progress: gameRunTicks skips the tick work and calls
    benchFrame after every frame. */
extern u8 benchActive;

/*  Stations measured so far. benchFps10[i] is valid for i < benchDone; the
    results screen shows the rest as unmeasured after an Esc. */
extern u8 benchDone;
extern u16 benchFps10[MAP_MAX_STATIONS];

/*  Frames over ticks for the whole run, in tenths. Set when the last station
    completes; 0 while running and after an abort. */
extern u16 benchAvg10;

/*  Load the benchmark map and start at its first station. FALSE, with the
    mode back in the menu, if the map fails to load or has no stations. */
u16 benchStart(void);

/*  Once per rendered frame while active, with the tick the frame's catch-up
    ran to. Times the station, moves to the next, and ends the run. */
void benchFrame(const u16 realTime);

/*  Abandon the run. What was measured stays for the results screen. */
void benchStop(void);

#endif
