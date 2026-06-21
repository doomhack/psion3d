#ifndef FP_MATH_H
#define FP_MATH_H

#include <plib.h>

#include "fp_types.h"

#define TRIG_TABLE_LEN 1024
#define TRIG_TABLE_MASK (TRIG_TABLE_LEN - 1)
#define TRIG_COS_OFFSET (TRIG_TABLE_LEN / 4)
#define TRIG_RAD_TO_INDEX 163
#define TRIG_RAD_TO_INDEX_SHIFT 8

extern const f16 sincos_tab[TRIG_TABLE_LEN];

static s16 trigidx(f16 rad)
{
	return (s16)(((s32)rad * TRIG_RAD_TO_INDEX) >> TRIG_RAD_TO_INDEX_SHIFT);
}

static f16 fpsin(f16 rad)
{
	return sincos_tab[trigidx(rad) & TRIG_TABLE_MASK];
}

static f16 fpcos(f16 rad)
{
	return sincos_tab[(trigidx(rad) + TRIG_COS_OFFSET) & TRIG_TABLE_MASK];
}

#endif