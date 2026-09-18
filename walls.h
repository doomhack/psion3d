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
   write.

   The elements were built behind ablation switches and measured on hardware,
   map 1, each alone against a 20fps baseline with all of them off: ceiling
   line 20 (as a cornice band, the same one span), stripes 19, skirting 19.5
   (as gated vent boxes; the gaps were narrower than a footprint beyond a
   cell, so it became a plain band). A one pixel joint between panels, since
   dropped, measured 19.5 gated to footprints of 16 or narrower, 19 with no
   cutoff when grazing corridor walls put one in nearly every column, and 15
   through bmFillRect (a fillSpan call per row) rather than bmFillCol1. The
   per hit footprint arithmetic in the ray loop is inside the all-off
   baseline and cost nothing measurable. The switches are gone now the set is
   settled; to re-measure an element, comment out its span call in
   drawConcretePanels() and read the fps counter. */

/* Depth in cells below which the panel wall draws its full detail: two stripes
   with a gap of wall between, and the skirting. From here to WALL_DETAIL_DEPTH
   the stripes are a row each anyway, so the group collapses to one band and
   the skirting is dropped. */
#define LAB_PANEL_NEAR 4

/* Widest column footprint, as f_wallXHalf, on which a lab style still draws a
   feature placed by footprint on a single face position - a locker upright,
   a conduit. Such a feature is drawn by every column whose footprint holds
   it, and down a corridor a column covers most of a cell, so every column
   would hold it and the run would go solid with it. Past this the feature
   is dropped; 24 is under half a locker door. */
#define LAB_LINE_HALF 24

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


typedef u16 (*wall_draw_fn)(u16 x, wallhit_t* hit);

extern wall_draw_fn drawWall;

u16 drawWallDefault(u16 x, wallhit_t* hit);
u16 drawWallLab(u16 x, wallhit_t* hit);

#endif
