#ifndef DDAASM_H
#define DDAASM_H

#include "fp_types.h"

/*  One ray's DDA between the cells it stops at, for draw.c.

    The walk is assembler (ddaasm.a) because C cannot hold it in registers:
    it has six live values - the cell pointer, both side distances, both
    deltas and the cell just read - which is the V30's six general
    registers, and TopSpeed, given the loop as its own function, still kept
    two of them on the stack and never used DI (the `register` hint changes
    nothing). pc/src/ddaasm_pc.c is the same walk in C for the PC host.

    The cell is a pointer into map[][] with no bounds check. That is safe
    because every map's edge is solid - loadMapFile refuses one that is not,
    and nothing at run time opens a solid cell - so no ray can step off the
    grid.

    All two-byte members, so the assembler's offsets are 2n whatever the
    packing: 0 cell, 2 sidedx, 4 sidedy, 6 deltax, 8 deltay. Side distances
    and deltas are unsigned and wrap exactly as the C did. */
typedef struct ddaray_t
{
	const u16* cell;	/* the map[][] entry of the cell the ray is in */
	u16 sidedx, sidedy;
	u16 deltax, deltay;
} ddaray_t;

/*  Step the ray - x when sidedx < sidedy, else y - until it enters a cell
    draw() has work at: a wall (MAP_MASK_WALL), or a sprite not yet
    collected (MAP_MASK_SPRITE without MAP_MASK_MARKED). Leaves the ray's
    cell and side distances at that stop and returns the face it came in
    through: 0 an x face, 1 a y face. The ray always steps at least once.

    One entry point per quadrant, so the direction is chosen once per ray
    (draw() sets the index in the branches that pick the step signs) and not
    at every stop: bit 0 of the index is stepping -x, bit 1 stepping -y.
    They save no general register - a stop is where the cost is now,
    34us a call when they saved five (TASKS.md task 26) - so the compiler
    keeps only what it needs across the call. The function
    pointer type is declared under the same pragma, so a call through it
    uses the same convention. */
#pragma save, call(reg_param=>(ax), reg_saved=>(es,ds,st1,st2))
typedef u16 (*ddawalk_fn)(ddaray_t* ray);

u16 ddaWalk0(ddaray_t* ray);	/* +x +y */
u16 ddaWalk1(ddaray_t* ray);	/* -x +y */
u16 ddaWalk2(ddaray_t* ray);	/* +x -y */
u16 ddaWalk3(ddaray_t* ray);	/* -x -y */
#pragma restore

#endif
