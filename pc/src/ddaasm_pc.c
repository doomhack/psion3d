/*  ddaasm_pc.c - portable replacement for ddaasm.a.

    The device walks the DDA in assembler so all six of its live values stay
    in registers, which TopSpeed would not do (ddaasm.h). This is the same
    walk in C: the loop is specialised per quadrant, as the assembler is, so
    each copy steps its pointer by constants. Frames rendered through it
    match the goldens the C loop in draw() produced before the walk moved
    out, so it is the reference the assembler is held to. */

#include "game_map.h"
#include "ddaasm.h"

#define DDA_WALK(cellStepX, cellStepY) \
	for(;;) \
	{ \
		if(sidedx < sidedy) \
		{ \
			sidedx += deltax; \
			cell += (cellStepX); \
			c = *cell; \
			if((c & MAP_MASK_WALL) || (c & (MAP_MASK_SPRITE | MAP_MASK_MARKED)) == MAP_MASK_SPRITE) \
			{ \
				side = 0; \
				break; \
			} \
		} \
		else \
		{ \
			sidedy += deltay; \
			cell += (cellStepY); \
			c = *cell; \
			if((c & MAP_MASK_WALL) || (c & (MAP_MASK_SPRITE | MAP_MASK_MARKED)) == MAP_MASK_SPRITE) \
			{ \
				side = 1; \
				break; \
			} \
		} \
	}

/* One walker per quadrant, as the assembler has, each stepping its pointer by
   constants. */
#define DDA_WALKER(name, cellStepX, cellStepY) \
	u16 name(ddaray_t* ray) \
	{ \
		const u16* cell = ray->cell; \
		u16 sidedx = ray->sidedx; \
		u16 sidedy = ray->sidedy; \
		const u16 deltax = ray->deltax; \
		const u16 deltay = ray->deltay; \
		u16 c; \
		u16 side; \
		DDA_WALK(cellStepX, cellStepY); \
		ray->cell = cell; \
		ray->sidedx = sidedx; \
		ray->sidedy = sidedy; \
		return side; \
	}

DDA_WALKER(ddaWalk0, 1, MAP_X)
DDA_WALKER(ddaWalk1, -1, MAP_X)
DDA_WALKER(ddaWalk2, 1, -MAP_X)
DDA_WALKER(ddaWalk3, -1, -MAP_X)
