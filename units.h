#ifndef UNITS_H
#define UNITS_H

#include "fp_types.h"

#define SYSTEM_TICKS_PER_SECOND 32
#define TICKS_PER_SECOND 32

#define MAP_CELL_METERS 2
#define MAP_UNITS_TO_METERS (int2fp(MAP_CELL_METERS))
#define METERS_TO_MAP_UNITS (flt2fp(0.5f))

#define FP_MAP_TO_METERS(x) fpmul((x), MAP_UNITS_TO_METERS)
#define FP_METERS_TO_MAP(x) fpmul((x), METERS_TO_MAP_UNITS)
#define FP_METERS_PER_SECOND_TO_MAP_TICK(x) (FP_METERS_TO_MAP(x) / TICKS_PER_SECOND)
#define FP_RADIANS_PER_SECOND_TO_TICK(x) ((x) / TICKS_PER_SECOND)

#define METERS_TO_MAP_CELLS(x) ((x) / MAP_CELL_METERS)
#define SECONDS_TO_TICKS(x) ((x) * TICKS_PER_SECOND)

/* Dividing a Q8 second count by this yields ticks: (1 << FP_BITS) / 32 == 8. */
#define FP_TICKS_DIVISOR ((1 << FP_BITS) / TICKS_PER_SECOND)

static u16 fpSecondsToTicks(const f16 f_seconds)
{
	if(f_seconds <= 0)
		return 0;

	/* This was fpmul(f_seconds, int2fp(TICKS_PER_SECOND)), which built the tick
	   count itself as a Q8 f16 - and f16 stops at 127.996, so any period of four
	   seconds or more wrapped negative, tripped the guard above and returned 0.
	   fpMetersPerSecondToCellTicks then clamped that 0 up to a one tick stride,
	   ie. a whole map cell per tick. The heavy at 0.5 m/s needs exactly four
	   seconds to cross a cell and so fell straight into it.

	   Dividing keeps every intermediate in 16 bits and has no ceiling below the
	   u16 this returns. */
	return (u16)(((u16)f_seconds + (FP_TICKS_DIVISOR / 2)) / FP_TICKS_DIVISOR);
}

static u8 fpMetersPerSecondToCellTicks(const f16 f_mps)
{
	u16 ticks;
	f16 f_cellSeconds;

	if(f_mps <= 0)
		return 0;

	f_cellSeconds = fpdiv(int2fp(MAP_CELL_METERS), f_mps);
	ticks = fpSecondsToTicks(f_cellSeconds);

	if(ticks == 0)
		return 1;

	if(ticks > 255)
		return 255;

	return (u8)ticks;
}

#endif
