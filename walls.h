#ifndef WALLS_H
#define WALLS_H

#include "fp_types.h"

typedef struct wallhit_t
{
	s16 wallHeight;
	f16 f_wallDist;
	f16 f_wallX;
	u16 cell;
	u8 side;
} wallhit_t;

typedef u16 (*wall_draw_fn)(u16 x, wallhit_t* hit);

extern wall_draw_fn drawWall;

u16 drawWallDefault(u16 x, wallhit_t* hit);

#endif
