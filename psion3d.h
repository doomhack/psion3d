#ifndef PSION3D_H
#define PSION3D_H

#include <plib.h>
#include <wlib.h>

#include "fp_types.h"
#include "fp_math.h"
#include "game_map.h"

static const u16 BM_BLK = 0;
static const u16 BM_GRY = 1;

static const P_RECT gameWinRect = {{0,0}, {240,160}};
static const P_RECT gameBitmapRect = {{0,0}, {256,320}};


typedef struct position_t
{
	f16 x, y;
	f16 angle;
} position_t;

extern position_t pos;

extern f16 dbgval;

#endif
