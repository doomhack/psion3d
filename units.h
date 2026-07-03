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

static u16 fpSecondsToTicks(const f16 f_seconds)
{
	f16 f_ticks = fpmul(f_seconds, int2fp(TICKS_PER_SECOND));

	if(f_ticks <= 0)
		return 0;

	return (u16)fp2int(f_ticks + flt2fp(0.5f));
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
