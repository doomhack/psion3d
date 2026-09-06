#ifndef DRAW_H
#define DRAW_H

#include "fp_types.h"

/* Where an enemy's round was aimed. A miss streaks past to one side so that
   being shot at and being hit do not look the same. */
#define TRACER_AIM_HIT 0
#define TRACER_AIM_WIDE_L 1
#define TRACER_AIM_WIDE_R 2

void draw(void);
void addEnemyTracer(u8 enemyId, u8 aim);

#endif