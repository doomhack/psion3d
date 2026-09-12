#ifndef WALLS_H
#define WALLS_H

#include "fp_types.h"

/* Distance in map cells at or beyond which a wall is drawn as a single flat
   span instead of the full detail set. Wall drawing is dominated by the number
   of span calls per column rather than by rows written, so lowering this is the
   cheapest lever on it - the cost is losing dither detail at medium range.
   Baseline is 6; try 5, 4 and 3 and read the fps counter. */
#define WALL_DETAIL_DEPTH 6

/* Lab SOLID wall - the concrete panel style in labwall.c. Each element is a
   span call per column, and the budget it was designed to is the measurement
   it replaced: three full width dither bands on the old brick column cost
   5.6ms per frame, about 2.6x the per row cost of the full height grey span
   they sat on, because the call and its clip preamble outweigh the rows they
   write. Set any of these to 0 to ablate that element and read the fps
   counter. The seams and vents are gated on the column's footprint on the
   face, so most columns pay nothing for them.

   Measured on hardware, map 1, each element alone against a 20fps baseline
   with all four off: cornice 20 (it replaced the old top band one for one),
   stripes 19, vents 19.5, seams 19.5 with LAB_PANEL_JOINT_HALF at 16. The
   seams were 15 through bmFillRect (a fillSpan call per row) and 19 through
   bmFillCol1 with no footprint cutoff, when grazing corridor walls put a
   joint in nearly every column; unrolling the wallSpans() tests changed
   nothing. The per hit footprint arithmetic in the ray loop is inside the
   all-off baseline and cost nothing measurable. */
#define LAB_PANEL_CORNICE 0 /* dark band along the ceiling line */
#define LAB_PANEL_STRIPES 1 /* stripe group at eye height */
#define LAB_PANEL_SEAMS 1   /* one pixel joint between panels, two per cell */
#define LAB_PANEL_VENTS 1   /* vent boxes along the skirting */

/* Widest column footprint, as f_wallXHalf, that still draws a panel joint.
   Joints are 128 wallX apart, so at a half width of 16 they are four columns
   apart and a panel is at least three columns wide; at 32 every other column
   is a joint. That is a comb rather than panelling, and it is also where the
   cost is: down a corridor the side walls are seen at 60-85 degrees, a
   column there covers most of a cell, and nearly every column pays a span
   call for a joint. Measured at 19fps with no cutoff and 19.5 at 16, against
   20 with joints off. Face on, 16 draws joints out to about 3.5 cells. */
#define LAB_PANEL_JOINT_HALF 16

/* Depth in cells below which the panel wall draws its full detail: two stripes
   with a gap of wall between, seams, and vents. From here to WALL_DETAIL_DEPTH
   the stripes are a row each anyway, so the group collapses to one band and
   the gated elements are dropped. */
#define LAB_PANEL_NEAR 4

typedef struct wallhit_t
{
	s16 wallHeight;
	f16 f_wallDist;
	f16 f_wallX;
	f16 f_wallXHalf; //Half the wallX the column covers along the face: its footprint is [f_wallX - half, f_wallX + half). Never 0.
	u16 cell;
	u8 mapX; //Cell coordinates, for wall types whose look depends on their neighbours.
	u8 mapY;
	u8 side;
	u8 wallXMirror; //TRUE when wallX runs right to left across the screen, so the footprint's high edge is the column's left pixel.
} wallhit_t;

/* True when the column's footprint on the face overlaps the wallX range
   [a, b). A point test on f_wallX is wrong for any feature narrower than a
   column's footprint: at range the columns sample the face more coarsely than
   the feature is wide, so it lands in no column at all and shimmers or vanishes
   as the player moves. The footprint test puts it in at least one column at any
   distance, and a feature covering several columns still fills every one. */
static u16 wallSpans(const wallhit_t* hit, s16 a, s16 b)
{
	return hit->f_wallX - hit->f_wallXHalf < b && hit->f_wallX + hit->f_wallXHalf > a;
}

/* Which of the column's four pixels the face position wallx falls in, 0 to 3
   from the left. Only meaningful when wallSpans(hit, wallx, wallx + 1) holds.
   This is the finest thing the footprint gives: a feature narrower than any
   column's footprint can be drawn a pixel wide at the pixel it belongs in, so
   it neither swims between columns nor fattens as the wall approaches. One
   16-bit divide, so it belongs in columns that have already passed wallSpans,
   not in every column. */
static s16 wallPixel(const wallhit_t* hit, s16 wallx)
{
	s16 d = hit->wallXMirror ?
		(hit->f_wallX + hit->f_wallXHalf - 1 - wallx) :
		(wallx - hit->f_wallX + hit->f_wallXHalf);
	s16 px = (d << 1) / hit->f_wallXHalf;

	return px > 3 ? 3 : px;
}

typedef u16 (*wall_draw_fn)(u16 x, wallhit_t* hit);

extern wall_draw_fn drawWall;
extern u8 wallFrame; /* Bumped once per draw(); see draw.c. */

u16 drawWallDefault(u16 x, wallhit_t* hit);
u16 drawWallLab(u16 x, wallhit_t* hit);

#endif
