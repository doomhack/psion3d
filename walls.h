#ifndef WALLS_H
#define WALLS_H

#include "fp_types.h"

/* Distance in map cells at or beyond which a wall is drawn as a single flat
   span instead of the full detail set. Wall drawing is dominated by the number
   of span calls per column rather than by rows written, so lowering this is the
   cheapest lever on it - the cost is losing dither detail at medium range.
   Baseline is 6; try 5, 4 and 3 and read the fps counter. */
#define WALL_DETAIL_DEPTH 6

/* Horizontal dither bands drawn across a brick column, 0 to 3. Each one is a
   separate span call, and all three measured 5.6ms per frame - about 2.6x the
   per row cost of the full height grey span they sit on, because the call and
   its clip preamble outweigh the ~0.3 x height of rows they write. They are
   emitted most visible first, so lowering this drops the least noticeable band
   first. The bands are disjoint, so the order does not affect the result. */
#define BRICK_BAND_COUNT 3

typedef struct wallhit_t
{
	s16 wallHeight;
	f16 f_wallDist;
	f16 f_wallX;
	u16 cell;
	u8 mapX; //Cell coordinates, for wall types whose look depends on their neighbours.
	u8 mapY;
	u8 side;
} wallhit_t;

typedef u16 (*wall_draw_fn)(u16 x, wallhit_t* hit);

extern wall_draw_fn drawWall;

u16 drawWallDefault(u16 x, wallhit_t* hit);
u16 drawWallLab(u16 x, wallhit_t* hit);

#endif
