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

/*  The profiling build (profile.pr, built by tools\profile.bat): every
    station is measured once per pass, each pass switching part of the frame
    off or doing it twice, and the results screen shows what each part costs
    in milliseconds. Only work is ever removed or repeated, never replaced,
    so nothing downstream of a switch changes (CLAUDE.md, Performance).
    None of it may reach PSION3D.IMG: the switches in gameloop.c and draw.c
    sit in #ifdef BENCH_PROFILE, because TopSpeed compiles `if(!0) f();`
    differently from `f();` there. BENCH_SKIP is 0 outside the build, which
    is safe only where a hash has shown it (psion3d.c). */
#define BENCH_NO_CLEAR 0x01	/* bmClearScreen */
#define BENCH_NO_BLIT 0x02	/* the platform's copy to the LCD */
#define BENCH_NO_WEAPON 0x04	/* the weapon overlay */
#define BENCH_NO_SPRITES 0x08	/* drawing the world sprites; collection stays */
#define BENCH_NO_WALLS 0x10	/* every drawWall call */
#define BENCH_WALLS_TWICE 0x20	/* every drawWall call made twice */
#define BENCH_NO_DRAW 0x40	/* draw() as a whole */

/*  What the passes resolve into, per station, in tenths of a millisecond a
    frame. Rays is the cast itself: DDA, hit maths, sprite collection and
    projection. Other is the frame loop around the render - wFlush, the
    HUD, input and the event poll. Residue is the frame less the sum of the
    rest, a check that the parts are separable: it should sit within the
    noise of zero. */
#define BENCH_PART_FRAME 0
#define BENCH_PART_RAYS 1
#define BENCH_PART_WALLS 2
#define BENCH_PART_SPRITES 3
#define BENCH_PART_WEAPON 4
#define BENCH_PART_CLEAR 5
#define BENCH_PART_BLIT 6
#define BENCH_PART_OTHER 7
#define BENCH_PART_RESIDUE 8
#define BENCH_PARTS 9

#ifdef BENCH_PROFILE

#define BENCH_PASSES 8
#define BENCH_PASS_TICKS (6 * TICKS_PER_SECOND)

/*  The switches for the pass in progress; 0 outside a run. */
extern u16 benchSkip;

#define BENCH_SKIP(bits) (benchSkip & (bits))

/*  The clear and the render of a benchmark frame, under the switches. */
void benchDraw(void);

/*  A part's cost at station i, for i < benchDone. Signed: a part near zero
    can measure slightly negative. */
s16 benchPartMs10(const u8 station, const u8 part);

#else

#define BENCH_SKIP(bits) 0

#endif

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
