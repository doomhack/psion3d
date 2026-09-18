#include "psion3d.h"
#include "bitmap.h"
#include "game_map.h"
#include "walls.h"

/* Lab wall set. All detail is made from solid, clear, or dithered spans.

   The set is built on the concrete panel wall, and the pieces of that wall
   the other types carry - the face, the ceiling line, the stripes at eye
   height, the skirting - are the helpers just below, so a feature wall is
   the panel with something set into it rather than its own copy of it. */

/* The skirting band, and the unit the panel details are sized in: a
   sixteenth of the wall, never less than a row. */
static s16 skirtBand(s16 h)
{
	s16 band = h >> 4;

	return band < 1 ? 1 : band;
}

/* The panel face, on the grey plane only: grey, or the pale dither when it
   is seen face on and near. Nothing is written to the black plane, so on a
   column that had something drawn behind it the caller clears that first.

   On overdraw, here and below: the black plane sits over the grey, so grey
   written under a black band is work thrown away. A span call costs about
   six rows (the measurement in CLAUDE.md), so a span is trimmed where a
   black band meets its end - the ceiling line, the skirting, a rail - and
   left whole where the band falls in its middle, where skipping the rows
   would take a second call. */
static void panelFace(s16 x, s16 y, s16 h, const wallhit_t* hit)
{
	if(fp2int(hit->f_wallDist) >= LAB_PANEL_NEAR || hit->side)
		bmFillRect4(x, y, h, greyBm);
	else
		bmFillPattern4(x, y, h, greyBm);
}

/* One row of black where the wall meets the ceiling. A row is the same call
   as the cornice band it replaced, which measured free. */
static void ceilingLine(s16 x, s16 y)
{
	bmFillRect4(x, y, 1, blackBm);
}

/* The black skirting along the foot of the wall. This began as a row of vent
   boxes with gaps gated on the footprint, but a gap of 8 wallX is narrower
   than a column's footprint beyond about a cell, so the gaps only ever showed
   up close and every column paid the test. A plain band is the same call
   without it. */
static void skirting(s16 x, s16 y, s16 h)
{
	s16 band = skirtBand(h);

	bmFillRect4(x, y + h - band, band, blackBm);
}

/* The panel's trim: ceiling line, the stripe group at eye height and the
   skirting, gated by depth. Inside LAB_PANEL_NEAR the stripes are two bands
   with a gap of bare wall between them, and that gap is the third stripe for
   free: black, wall, dither read as three bands for two calls. From there to
   WALL_DETAIL_DEPTH the stripes are a row each anyway, so the group collapses
   to one band and the skirting is dropped. The group hangs off row 80 like
   the old horizon band did, so it is always on screen and never clips, and it
   is the same on every cell, so a corridor reads as one run. */
static void panelTrim(s16 x, s16 y, s16 h, const wallhit_t* hit)
{
	s16 band = skirtBand(h);

	ceilingLine(x, y);

	if(fp2int(hit->f_wallDist) >= LAB_PANEL_NEAR)
	{
		bmFillRect4(x, 80 - band - band - band, band + band + band, blackBm);

		return;
	}

	bmFillRect4(x, 80 - band - band - band, band, blackBm);
	bmFillPattern4(x, 80 - band, band, blackBm);
	skirting(x, y, h);
}

/* Prefabricated concrete panels: the face and its trim. In span calls per
   column, which is what wall cost is:

     depth >= WALL_DETAIL_DEPTH  flat                          1
     depth >= LAB_PANEL_NEAR     face, ceiling, one dark band  3
     nearer                      face, ceiling, two stripes,   5
                                 skirting

   There is nothing here narrower than a column. A one pixel joint between
   panels was tried: it has to land in exactly one column, and a column only
   knows its centre sample and an overlapping estimate of its footprint, so
   keeping it single needed per frame dedupe state that then had to survive
   two faces interleaving through a grille. The joint went instead. */
static u16 drawConcretePanels(s16 x, s16 y, s16 h, const wallhit_t* hit)
{
	s16 depth = fp2int(hit->f_wallDist);

	if(depth >= WALL_DETAIL_DEPTH)
	{
		bmFillRect4(x, y, h, blackBm);

		return TRUE;
	}

	/* The face stops short of the ceiling line, and of the skirting when
	   there is one. */
	panelFace(x, y + 1, h - 1 - (depth < LAB_PANEL_NEAR ? skirtBand(h) : 0), hit);
	panelTrim(x, y, h, hit);

	return TRUE;
}

/* Shootable wall: a concrete panel with a large grille set into it, big enough
   to be the way through once it is shot out. The cell is not solid, so the
   ray ran on and whatever stands behind it was drawn into this column first;
   the grille writes the black plane only, so that shows through the slats,
   which is the tell that the wall is worth a round.

   The outer quarter of the face on each side is the standard panel, on a
   column cleared of the wall behind. The middle half is a header and sill of
   panel carrying the ceiling line and skirting across, and between them the
   opening. Near, five slats - a rail against the header, one against the
   sill, three between - with the wall behind in the gaps; from LAB_PANEL_NEAR
   out a dither mesh, one span, in step with the panel's stripes collapsing at
   that depth. Beyond WALL_DETAIL_DEPTH the wall behind is black too, so the
   column is one black span.

   The frame down each side is a stile the width of a column, drawn by every
   column whose footprint holds the opening's edge - one column, or two where
   neighbouring footprints overlap on it. That is the grain of the renderer:
   a thinner stile has to pick one column, which the footprint cannot do on
   its own, and the dedupe state it took was not worth a few pixels. The
   opening between the stiles is a point test, which is safe for a feature
   this much wider than any footprint at detail depth.

   The opening's columns return FALSE, leaving them non occluding as BARS and
   WINDOW do: the depth buffer holds one distance per column, and a sprite
   behind the grille drawing over it is the lesser error against it vanishing
   in the gaps. */

#define VENT_LEFT 64   /* wallX extent of the opening: the middle half */
#define VENT_RIGHT 192

static u16 drawVentPanel(s16 x, s16 y, s16 h, const wallhit_t* hit)
{
	s16 depth = fp2int(hit->f_wallDist);
	s16 wallx = hit->f_wallX;
	s16 band = skirtBand(h);
	s16 ventY, ventH, sillY, slat, pitch;
	u16 stile;

	if(depth >= WALL_DETAIL_DEPTH)
	{
		bmFillRect4(x, y, h, blackBm);

		return TRUE;
	}

	ventY = y + (h >> 2);
	sillY = y + h - band - band;
	ventH = sillY - ventY;

	stile = wallSpans(hit, VENT_LEFT, VENT_LEFT + 1) ||
		wallSpans(hit, VENT_RIGHT - 1, VENT_RIGHT);

	if(wallx < VENT_LEFT || wallx >= VENT_RIGHT)
	{
		bmClearRect4(x, y, h, blackBm);
		drawConcretePanels(x, y, h, hit);

		if(stile)
			bmFillRect4(x, ventY, ventH, blackBm);

		return TRUE;
	}

	/* Header and sill, the same face as the panel either side, between the
	   ceiling line and the skirting. */
	bmClearRect4(x, y + 1, ventY - y - 1, blackBm);
	bmClearRect4(x, sillY, y + h - band - sillY, blackBm);
	panelFace(x, y + 1, ventY - y - 1, hit);
	panelFace(x, sillY, y + h - band - sillY, hit);
	ceilingLine(x, y);
	skirting(x, y, h);

	if(stile)
	{
		bmFillRect4(x, ventY, ventH, blackBm);

		return TRUE;
	}

	if(depth >= LAB_PANEL_NEAR)
	{
		bmFillPattern4(x, ventY, ventH, blackBm);

		return FALSE;
	}

	/* Five slats: a rail against the header, one against the sill, and three
	   between at a quarter of the height each. */
	slat = ventH >> 4;

	if(slat < 1)
		slat = 1;

	pitch = ventH >> 2;

	bmFillRect4(x, ventY, slat, blackBm);
	bmFillRect4(x, ventY + pitch, slat, blackBm);
	bmFillRect4(x, ventY + pitch + pitch, slat, blackBm);
	bmFillRect4(x, ventY + pitch + pitch + pitch, slat, blackBm);
	bmFillRect4(x, ventY + ventH - slat, slat, blackBm);

	return FALSE;
}

/* Archway: an opening in the panel wall. The jambs, the outer 32 wallX each
   side, are concrete panel columns on a column cleared of what the ray found
   beyond, so the ceiling line, stripes and skirting run up to the opening;
   two arches side by side share a pier. Over the opening a lintel of panel
   face carries the ceiling line across, with a black beam along its
   underside, and a black stile down each jamb's inner edge - a column wide,
   on whichever columns' footprints hold the edge, as the grille's are - so
   the opening is framed even when the wall beyond is the same grey as the
   jambs. The cell is not solid, so the opening's columns are otherwise left
   as the ray drew what stands behind them. Gated as the panels are: beyond
   WALL_DETAIL_DEPTH the lintel is one black span and there are no stiles.

   The jambs are drawn on the cell face, so the arch has no depth: seen
   obliquely it is a flat opening. Real jambs, found by the ray caster as
   boxes in the cell, were tried and taken out again: from in front of a
   doorway their reveals fill both sides of the screen with full height
   wall, which measured 13fps against 20 on the device. */

#define ARCH_JAMB 32 /* wallX of jamb at each side of the cell */

static u16 drawArchway(s16 x, s16 y, s16 h, const wallhit_t* hit)
{
	s16 depth = fp2int(hit->f_wallDist);
	s16 wallx = hit->f_wallX;
	s16 band = skirtBand(h);
	s16 lintelH = h >> 2;

	u16 stile = depth < WALL_DETAIL_DEPTH &&
		(wallSpans(hit, ARCH_JAMB, ARCH_JAMB + 1) ||
		wallSpans(hit, 256 - ARCH_JAMB - 1, 256 - ARCH_JAMB));

	if(wallx < ARCH_JAMB || wallx >= 256 - ARCH_JAMB)
	{
		bmClearRect4(x, y, h, blackBm);
		drawConcretePanels(x, y, h, hit);

		if(stile)
			bmFillRect4(x, y + lintelH, h - lintelH, blackBm);

		return TRUE;
	}

	if(depth >= WALL_DETAIL_DEPTH)
	{
		bmFillRect4(x, y, lintelH, blackBm);

		return FALSE;
	}

	/* Lintel: panel face under the ceiling line, the beam along its underside
	   - which on a stile column runs on down to the floor. */
	bmClearRect4(x, y + 1, lintelH - 1 - band, blackBm);
	panelFace(x, y + 1, lintelH - 1 - band, hit);
	ceilingLine(x, y);

	if(stile)
	{
		bmFillRect4(x, y + lintelH - band, h - lintelH + band, blackBm);

		return TRUE;
	}

	bmFillRect4(x, y + lintelH - band, band, blackBm);

	return FALSE;
}

/* Airlock door. A fixed surround - black outer edge, grey frame, black lip and
   a grey recess - holds two pale leaves that slide apart into the recess as the
   player approaches. Leaf detail is placed relative to the leaf's own inner
   edge, so it travels with the leaf as the door opens. wallX runs 0..255 across
   the wall face. */

#define DOOR_FRAME_W 38                  /* fixed surround width, each side */
#define DOOR_TRAVEL (128 - DOOR_FRAME_W) /* leaf width, and its full travel */
#define DOOR_STILE 7                     /* meeting stile, inward from the seam */

static void airlockFrame(s16 x, s16 y, s16 h, s16 fx, s16 lip)
{
	/* Outer edge, and the lip dividing frame from recess, are solid. */
	if(fx < 6 || (fx >= 26 && fx < 30))
	{
		bmFillRect4(x, y, h, blackBm);
		return;
	}

	bmClearRect4(x, y, h, blackBm);
	bmFillRect4(x, y, h, greyBm);
	bmFillRect4(x, y, lip, blackBm);
	bmFillRect4(x, y + h - lip, lip, blackBm);
}

/* Header and sill. They belong to the fixed surround, so they are drawn whether
   the leaves are shut or wide open, and the edge lips carry the frame's black
   outline across the opening. The caller owns the black plane clear, because
   how far it may reach depends on whether the doorway is open. */
static void airlockSurround(s16 x, s16 y, s16 h, s16 wallx, s16 leafY, s16 footY, s16 lip)
{
	s16 headerH = leafY - y;

	bmFillRect4(x, y, headerH, greyBm);
	bmFillRect4(x, footY, y + h - footY, greyBm);
	bmFillRect4(x, y, lip, blackBm);
	bmFillRect4(x, leafY - lip, lip, blackBm);
	bmFillRect4(x, footY, lip, blackBm);
	bmFillRect4(x, y + h - lip, lip, blackBm);

	/* Status lamps set into the header. */
	if((wallx >= 44 && wallx < 62) || (wallx >= 194 && wallx < 212))
		bmFillRect4(x, y + (headerH >> 1), headerH >> 2, blackBm);
}

static u16 drawAirlock(s16 x, s16 y, s16 h, const wallhit_t* hit, s16 halfgap)
{
	s16 wallx = hit->f_wallX;
	s16 headerH = h >> 3;
	s16 footH = h >> 4;
	s16 lip = h >> 6;
	s16 leafY, leafH, footY, handleY, handleH, d;

	if(lip < 1)
		lip = 1;

	if(wallx < DOOR_FRAME_W)
	{
		airlockFrame(x, y, h, wallx, lip);
		return TRUE;
	}

	if(wallx >= 256 - DOOR_FRAME_W)
	{
		airlockFrame(x, y, h, 255 - wallx, lip);
		return TRUE;
	}

	leafY = y + headerH;
	footY = y + h - footH;
	leafH = footY - leafY;

	if(wallx > 128 - halfgap && wallx < 128 + halfgap)
	{
		/* Open doorway. Clear the black plane under the header and sill only:
		   the wall seen through the gap was drawn into this column first, and a
		   full height clear would strip it back to its grey plane. */
		bmClearRect4(x, y, headerH, blackBm);
		bmClearRect4(x, footY, footH, blackBm);
		airlockSurround(x, y, h, wallx, leafY, footY, lip);

		return FALSE;
	}

	/* The leaf is opaque, so the whole column can be cleared in one call. */
	bmClearRect4(x, y, h, blackBm);
	airlockSurround(x, y, h, wallx, leafY, footY, lip);
	bmClearRect4(x, leafY, leafH, greyBm);

	d = (wallx < 128) ? (128 - halfgap - wallx) : (wallx - 128 - halfgap);

	if(d < DOOR_STILE)
	{
		bmFillRect4(x, leafY, leafH, blackBm);
		return TRUE;
	}

	/* Pale leaf face, dithered vision band across it, dark kick plate below. */
	bmFillPattern4(x, leafY + (leafH >> 2), leafH >> 3, blackBm);
	bmFillRect4(x, footY - (leafH >> 3), leafH >> 3, blackBm);

	if(d >= 10 && d < 22)
	{
		handleH = leafH >> 3;
		handleY = leafY + (leafH >> 1);

		bmFillRect4(x, handleY, handleH, greyBm);

		if(d >= 13 && d < 19)
			bmFillRect4(x, handleY + (handleH >> 2), handleH >> 1, blackBm);
	}

	return TRUE;
}

static u16 drawLockedAirlockDoor(s16 x, s16 y, s16 h, const wallhit_t* hit)
{
	return drawAirlock(x, y, h, hit, 0);
}

static u16 drawAirlockDoor(s16 x, s16 y, s16 h, const wallhit_t* hit)
{
	s16 dist = hit->f_wallDist >> 4;
	s16 doorgap, halfgap;

	/* An enemy at the door holds it fully open; otherwise it travels with the
	   player's approach. Same test as enemy line of sight, see walls.c. */
	if(doorEnemyNear(hit->mapX, hit->mapY))
		return drawAirlock(x, y, h, hit, DOOR_TRAVEL);

	if(dist >= 16)
		return drawAirlock(x, y, h, hit, 0);

	doorgap = 16 - dist;
	halfgap = (doorgap << 2) + (doorgap << 1);

	if(halfgap > DOOR_TRAVEL)
		halfgap = DOOR_TRAVEL;

	return drawAirlock(x, y, h, hit, halfgap);
}

/* Lockers: three to a cell, black doors between grey uprights, under a black
   top rail and standing on the panel skirting. Each door has a dithered vent
   near its top and another near its foot, and a pale handle slot at hand
   height towards one edge. The doors are point tests, being wide enough to be
   safe at any detail depth; the uprights and the handle are footprint tests,
   since both are narrower than a column at range. A footprint test fattens
   with the footprint, though: seen down a corridor a column covers most of a
   door, every column holds an upright's centre line, and the run went solid
   grey. So past LAB_LINE_HALF the uprights and handles are dropped and
   the run is doors and vents alone, which is at least the right colour.
   Inside LAB_PANEL_NEAR everything; out to WALL_DETAIL_DEPTH the handles go;
   beyond, one black span. The grey plane is left alone except under the
   uprights: a solid cell's column arrives clear, and the black fill is black
   wherever nothing is cleared out of it. */

#define LOCKER_DOOR_W 68
#define LOCKER_POST_W 16
#define LOCKER_PITCH (LOCKER_DOOR_W + LOCKER_POST_W)
#define LOCKER_EDGE 10 /* half an upright at each cell edge */
/* Centre line of the upright after door n. */
#define LOCKER_POST(n) (LOCKER_EDGE + LOCKER_DOOR_W + (LOCKER_POST_W >> 1) + (n) * LOCKER_PITCH)
#define LOCKER_VENT_IN 8   /* vent inset from each door edge */
#define LOCKER_HANDLE_IN 8 /* handle slot, from the door edge */
#define LOCKER_HANDLE_W 6

static u16 drawLockers(s16 x, s16 y, s16 h, const wallhit_t* hit)
{
	s16 depth = fp2int(hit->f_wallDist);
	s16 wallx = hit->f_wallX;
	s16 band = skirtBand(h);
	s16 ventH = h >> 3;
	s16 handleH = ventH;
	s16 d, doorX;

	bmFillRect4(x, y, h, blackBm);

	if(depth >= WALL_DETAIL_DEPTH)
		return TRUE;

	/* An upright is drawn by the columns whose footprint holds its centre
	   line: one column, two at most, whatever its nominal width. Tested on
	   the full width it came out three columns wide against three column
	   doors. */
	if(hit->f_wallXHalf <= LAB_LINE_HALF &&
		(wallSpans(hit, 0, 1) ||
		wallSpans(hit, LOCKER_POST(0), LOCKER_POST(0) + 1) ||
		wallSpans(hit, LOCKER_POST(1), LOCKER_POST(1) + 1) ||
		wallSpans(hit, 256, 257)))
	{
		bmClearRect4(x, y + band, h - band - band, blackBm);
		bmFillRect4(x, y + band, h - band - band, greyBm);

		return TRUE;
	}

	/* Fold the three doors onto one. */
	d = wallx - LOCKER_EDGE;

	if(d >= LOCKER_PITCH + LOCKER_PITCH)
		d -= LOCKER_PITCH + LOCKER_PITCH;
	else if(d >= LOCKER_PITCH)
		d -= LOCKER_PITCH;

	if(d >= LOCKER_VENT_IN && d < LOCKER_DOOR_W - LOCKER_VENT_IN)
	{
		bmFillPattern4(x, y + ventH, ventH, blackBm);
		bmFillPattern4(x, y + h - ventH - ventH, ventH, blackBm);
	}

	if(depth >= LAB_PANEL_NEAR || hit->f_wallXHalf > LAB_LINE_HALF)
		return TRUE;

	doorX = wallx - d;

	if(wallSpans(hit, doorX + LOCKER_HANDLE_IN, doorX + LOCKER_HANDLE_IN + LOCKER_HANDLE_W))
		bmClearRect4(x, y + (h >> 1) - (handleH >> 1), handleH, blackBm);

	return TRUE;
}

/* Glazed partition. A black frame divides the wall into three bays, each a
   transom over dithered glass over a solid spandrel, standing on the same
   skirting as the concrete panels. The glass writes the black plane only, so
   the wall behind keeps its grey and still shows through, and no span reaches
   outside a panel it paints over. The cell stays non occluding.

   Gated by depth as the concrete panels are: inside LAB_PANEL_NEAR the
   skirting and the mullions, beyond it neither - every column is a bay, so
   the partition reads as one run of glazing from a distance. */

#define BAY_PITCH 80 /* one bay plus one mullion, in wallX units */
#define BAY_W 64     /* glazed width of a bay, leaving 16 for the mullion */

static u16 drawGlazedPartition(s16 x, s16 y, s16 h, const wallhit_t* hit)
{
	s16 bx = hit->f_wallX - (BAY_PITCH - BAY_W);
	s16 rail = h >> 6;
	s16 transH = h >> 2;
	s16 glassY = y + transH;
	s16 spandY = y + h - transH;
	s16 depth = fp2int(hit->f_wallDist);
	s16 footH;

	if(rail < 1)
		rail = 1;

	/* Fold the three bays onto one pitch, so one range test covers the outer
	   frame and both mullions. */
	if(depth < LAB_PANEL_NEAR)
	{
		if(bx >= BAY_PITCH + BAY_PITCH)
			bx -= BAY_PITCH + BAY_PITCH;
		else if(bx >= BAY_PITCH)
			bx -= BAY_PITCH;

		if(bx < 0 || bx >= BAY_W)
		{
			bmFillRect4(x, y, h, blackBm);

			return FALSE;
		}
	}

	/* Transom: the face between its rails. */
	bmClearRect4(x, y + rail, transH - rail - rail, blackBm);
	bmFillRect4(x, y + rail, transH - rail - rail, greyBm);
	bmFillRect4(x, y, rail, blackBm);
	bmFillRect4(x, glassY - rail, rail, blackBm);

	bmFillPattern4(x, glassY, spandY - glassY, blackBm);

	/* Spandrel: the face between its rail and the panel skirting, when the
	   skirting is drawn. */
	footH = depth < LAB_PANEL_NEAR ? skirtBand(h) : 0;
	bmClearRect4(x, spandY + rail, transH - rail - footH, blackBm);
	bmFillRect4(x, spandY + rail, transH - rail - footH, greyBm);
	bmFillRect4(x, spandY, rail, blackBm);

	if(footH)
		skirting(x, y, h);

	return FALSE;
}

/* Observation window. Three nested frame rings - black, grey, then a thin black
   lip - set into a pale wall, with tinted glass inside, under the panel
   ceiling line and standing on the panel skirting. The glass is a grey dither
   and nothing else: the black plane sits over the grey, so the black of
   whatever stands behind the window shows through the tint, and no clear
   reaches across the pane. */

#define WIN_MARGIN 12 /* pale wall outside the frame, in wallX units */
#define WIN_OUTER 31  /* inner edge of the black ring */
#define WIN_MID 45    /* inner edge of the grey ring */
#define WIN_INNER 54  /* inner edge of the black lip: the glass starts here */

static u16 drawObservationWindow(s16 x, s16 y, s16 h, const wallhit_t* hit)
{
	s16 wallx = hit->f_wallX;
	s16 fx = (wallx < 128) ? wallx : (255 - wallx);
	s16 t1 = y + (h >> 4) + (h >> 5);
	s16 t2 = t1 + (h >> 4);
	s16 t3 = t2 + (h >> 4);
	s16 t4 = y + (h >> 2);
	s16 b4 = y + (h >> 1) + (h >> 3);
	s16 b3 = b4 + (h >> 5);
	s16 b2 = b3 + (h >> 4);
	s16 b1 = b2 + (h >> 4);
	s16 band = skirtBand(h);

	if(fx < WIN_INNER)
	{
		/* No glass in this column, so it is opaque and the rings can simply be
		   overdrawn onto a cleared column, outermost first. */
		bmClearRect4(x, y + 1, h - 1 - band, blackBm);
		bmClearRect4(x, y + 1, h - 1 - band, greyBm);
		ceilingLine(x, y);

		if(fx >= WIN_MARGIN)
			bmFillRect4(x, t1, b1 - t1, blackBm);

		if(fx >= WIN_OUTER)
		{
			bmClearRect4(x, t2, b2 - t2, blackBm);
			bmFillRect4(x, t2, b2 - t2, greyBm);
		}

		if(fx >= WIN_MID)
			bmFillRect4(x, t3, b3 - t3, blackBm);

		skirting(x, y, h);

		return TRUE;
	}

	/* Head: pale wall, then the three frame bands, each drawn as its own span so
	   that nothing has to be cleared back off again. */
	bmClearRect4(x, y + 1, t3 - y - 1, blackBm);
	bmClearRect4(x, y + 1, t1 - y - 1, greyBm);
	ceilingLine(x, y);
	bmFillRect4(x, t1, t2 - t1, blackBm);
	bmFillRect4(x, t2, t3 - t2, greyBm);
	bmFillRect4(x, t3, t4 - t3, blackBm);

	bmFillPattern4(x, t4, b4 - t4, greyBm);

	/* Sill: the three bands again, then the pale wall and its skirting. The
	   clears stop where a black band starts and where the skirting begins. */
	bmClearRect4(x, b3, y + h - band - b3, blackBm);
	bmClearRect4(x, b1, y + h - band - b1, greyBm);
	bmFillRect4(x, b4, b3 - b4, blackBm);
	bmFillRect4(x, b3, b2 - b3, greyBm);
	bmFillRect4(x, b2, b1 - b2, blackBm);
	skirting(x, y, h);

	return FALSE;
}

static u16 drawVoid(s16 x)
{
	bmFillRect4(x, 80, 1, blackBm);
	return FALSE;
}

/* Signboard on the concrete panel wall: a board three quarters of the cell
   wide and half the wall high, its top a fifth of the way down, in a black
   frame over the panel stripes, with four lines of lettering suggested by
   black bars of unequal length, centred: a centred line is symmetric about
   the face's middle, so it reads the same whichever way wallX runs on this
   face, and nothing here needs to know. The face clears both planes, which
   leaves the background shade - the brightest the display has - so a sign
   reads as lit across a room. The board is drawn at every distance for that reason; the
   lettering only inside LAB_PANEL_NEAR. */

#define SIGN_L 32        /* wallX extent of the board */
#define SIGN_R 224
#define SIGN_MID 128     /* the lettering is centred here, each line this wide either side */
#define SIGN_HALF1 80
#define SIGN_HALF2 56
#define SIGN_HALF3 72
#define SIGN_HALF4 40

static u16 drawSignboard(s16 x, s16 y, s16 h, const wallhit_t* hit)
{
	s16 wallx = hit->f_wallX;
	s16 boardY = y + (h >> 3) + (h >> 4) + (h >> 6); /* a fifth, near enough */
	s16 boardH = h >> 1;
	s16 rail = h >> 5;
	s16 faceY, faceH, lineY, pitch, tx;

	drawConcretePanels(x, y, h, hit);

	if(wallx < SIGN_L || wallx >= SIGN_R)
		return TRUE;

	if(rail < 1)
		rail = 1;

	faceY = boardY + rail;
	faceH = boardH - rail - rail;

	bmFillRect4(x, boardY, boardH, blackBm);
	bmClearRect4(x, faceY, faceH, blackBm);
	bmClearRect4(x, faceY, faceH, greyBm);

	if(fp2int(hit->f_wallDist) >= LAB_PANEL_NEAR)
		return TRUE;

	/* Distance from the face's middle, either way. */
	tx = wallx < SIGN_MID ? SIGN_MID - wallx : wallx - SIGN_MID;

	pitch = faceH >> 2;
	lineY = faceY + (pitch >> 1) - (rail >> 1);

	if(tx < SIGN_HALF1)
		bmFillRect4(x, lineY, rail, blackBm);

	if(tx < SIGN_HALF2)
		bmFillRect4(x, lineY + pitch, rail, blackBm);

	if(tx < SIGN_HALF3)
		bmFillRect4(x, lineY + pitch + pitch, rail, blackBm);

	if(tx < SIGN_HALF4)
		bmFillRect4(x, lineY + pitch + pitch + pitch, rail, blackBm);

	return TRUE;
}

/* Lit panels: the concrete panel wall under a strip light. The face is the
   pale dither the panels use face on and near, here at every depth and on
   both sides, under the same trim; the light itself is a white band near
   the top, in a black housing near. The lamp is drawn at every distance, as
   the one true highlight in the palette it is what makes the wall worth
   walking towards. Spans per column: the panel's plus one for the lamp,
   plus two for its housing near. */
static u16 drawLitPanels(s16 x, s16 y, s16 h, const wallhit_t* hit)
{
	s16 depth = fp2int(hit->f_wallDist);
	s16 rail = h >> 5;
	s16 lampY = y + (h >> 3);
	s16 lampH = skirtBand(h);

	if(rail < 1)
		rail = 1;

	if(depth >= WALL_DETAIL_DEPTH)
	{
		bmFillRect4(x, y, h, blackBm);
		bmClearRect4(x, lampY, lampH, blackBm);

		return TRUE;
	}

	bmFillPattern4(x, y + 1, h - 1 - (depth < LAB_PANEL_NEAR ? lampH : 0), greyBm);
	panelTrim(x, y, h, hit);
	bmClearRect4(x, lampY, lampH, greyBm);

	if(depth < LAB_PANEL_NEAR)
	{
		bmFillRect4(x, lampY - rail, rail, blackBm);
		bmFillRect4(x, lampY + lampH, rail, blackBm);
	}

	return TRUE;
}

/* Conduit runs down the concrete panel wall. Vertical detail, so a column
   either carries a run or it does not - one extra span at most, none on the
   wall between. The brackets sit inside the run's own columns, so they cost
   nothing anywhere else. The run is tested before its lit edge, so a column
   at range that covers both keeps the dark run; the edge is the bare screen,
   since grey on the grey panel would not show. A run or an edge is decided
   before the panel is drawn, so neither pays for a panel it then covers.
   Past LAB_LINE_HALF the runs are dropped and the wall is plain panel, for
   the reason given there. */
static u16 drawConduitRuns(s16 x, s16 y, s16 h, const wallhit_t* hit)
{
	s16 depth = fp2int(hit->f_wallDist);
	s16 bracket;

	if(hit->f_wallXHalf > LAB_LINE_HALF)
		return drawConcretePanels(x, y, h, hit);

	if(wallSpans(hit, 32, 48) ||
		wallSpans(hit, 104, 120) ||
		wallSpans(hit, 192, 208))
	{
		bmFillRect4(x, y, h, blackBm);

		if(depth >= LAB_PANEL_NEAR)
			return TRUE;

		bracket = h >> 6;

		if(bracket < 1)
			bracket = 1;

		/* Brackets. The black plane sits over the grey, so a pale band on a
		   black run has to clear black before it will show at all. */
		bmClearRect4(x, y + (h >> 2), bracket, blackBm);
		bmFillRect4(x, y + (h >> 2), bracket, greyBm);
		bmClearRect4(x, y + h - (h >> 2), bracket, blackBm);
		bmFillRect4(x, y + h - (h >> 2), bracket, greyBm);

		return TRUE;
	}

	/* Lit edge down the side of each run, so it reads as round: the bare
	   screen, which on a solid cell's column - drawn first, into a clear
	   column - is nothing drawn at all. */
	if(depth < WALL_DETAIL_DEPTH &&
		(wallSpans(hit, 48, 56) ||
		wallSpans(hit, 120, 128) ||
		wallSpans(hit, 208, 216)))
		return TRUE;

	return drawConcretePanels(x, y, h, hit);
}

/* Tiled wall: concrete panel over a dado of white tiles. The top three
   eighths are the panel face under the ceiling line; below that the tiles
   run to the skirting, the bare screen with a black grout course every
   eighth of the wall. The tiles replace the panel stripes, so the run reads
   as one surface with a change of material rather than a panel with a
   feature on it. The courses are near detail; from LAB_PANEL_NEAR out the
   tiles are one white band with an edge, and the skirting stays at every
   detail depth here, since a white band running into the floor would have
   no bottom. There are no vertical joints: a column wide, sometimes two,
   against courses one row deep, they read as bars beyond a cell and a half
   and as a grid only in the columns whose footprint happened to be narrow,
   so the courses carry the tiling alone. */
static u16 drawTiledDado(s16 x, s16 y, s16 h, const wallhit_t* hit)
{
	s16 depth = fp2int(hit->f_wallDist);
	s16 pitch = h >> 3;
	s16 tileY = y + (h >> 2) + (h >> 3);

	if(depth >= WALL_DETAIL_DEPTH)
	{
		bmFillRect4(x, y, h, blackBm);

		return TRUE;
	}

	panelFace(x, y + 1, tileY - y - 1, hit);
	ceilingLine(x, y);
	bmFillRect4(x, tileY, 1, blackBm);
	skirting(x, y, h);

	if(depth >= LAB_PANEL_NEAR)
		return TRUE;

	bmFillRect4(x, tileY + pitch, 1, blackBm);
	bmFillRect4(x, tileY + pitch + pitch, 1, blackBm);
	bmFillRect4(x, tileY + pitch + pitch + pitch, 1, blackBm);
	bmFillRect4(x, tileY + pitch + pitch + pitch + pitch, 1, blackBm);

	return TRUE;
}

/* A bench or partition low enough to see over. The cell is not solid, so the
   ray ran on and what stands behind it went into this column first; the bench
   is opaque, so it clears the black plane back off over its own half. It
   stands on the panel skirting, gated as the panels are, and beyond
   WALL_DETAIL_DEPTH it is one black span like every other wall.

   It returns FALSE, leaving the column non occluding. The depth buffer holds
   one distance per column and cannot say solid-below-row-100, so the choice is
   between a sprite behind the bench drawing over it and that sprite
   disappearing in the open air above it. The first is the lesser error and is
   what BARS and WINDOW already do here. */
static u16 drawBench(s16 x, s16 y, s16 h, const wallhit_t* hit)
{
	s16 top = y + (h >> 1);
	s16 lowH = h - (h >> 1);
	s16 coping = h >> 5;
	s16 footH;

	if(coping < 1)
		coping = 1;

	if(fp2int(hit->f_wallDist) >= WALL_DETAIL_DEPTH)
	{
		bmFillRect4(x, top, lowH, blackBm);

		return FALSE;
	}

	/* The face between the coping and the panel skirting, which is drawn on
	   the panel's gating. */
	footH = fp2int(hit->f_wallDist) < LAB_PANEL_NEAR ? skirtBand(h) : 0;
	bmClearRect4(x, top + coping, lowH - coping - footH, blackBm);
	bmFillRect4(x, top + coping, lowH - coping - footH, greyBm);
	bmFillRect4(x, top, coping, blackBm);

	if(footH)
		skirting(x, y, h);

	return FALSE;
}

/* A structural strut standing in the cell. The ray caster finds the post
   itself, a square in the cell's middle, so every column that arrives here
   is post and the cell's open part never does; see pillarHit() in draw.c.
   The post is opaque, so the column occludes. Its faces are shaded as the
   panels are - grey, or the pale dither face on and near - so where two
   faces meet at a corner the change of shade draws the edge. */
static u16 drawStrut(s16 x, s16 y, s16 h, const wallhit_t* hit)
{
	s16 band = skirtBand(h);
	s16 flange = band >> 1;

	if(flange < 1)
		flange = 1;

	if(fp2int(hit->f_wallDist) >= WALL_DETAIL_DEPTH)
	{
		bmFillRect4(x, y, h, blackBm);

		return TRUE;
	}

	/* The face between the cap and the base. No clear first: the post stops
	   the ray, so it is the first thing drawn into its column, and the
	   black plane is clear already. */
	panelFace(x, y + band, h - band - band, hit);

	/* Cap, base - the panel skirting - and two bolted flanges between. */
	bmFillRect4(x, y, band, blackBm);
	skirting(x, y, h);
	bmFillRect4(x, y + (h >> 2), flange, blackBm);
	bmFillRect4(x, y + h - (h >> 2), flange, blackBm);

	return TRUE;
}

/* Control box set into the concrete panel wall, fed by a conduit. A black
   framed, white faced housing centred on eye level, three eighths of the
   wall high and over the panel stripes, holding two things side by side: on
   the left a black slot with a pale lever block in it, on the right a grey
   panel with a lamp in a white bezel at its top and a small white label
   below. Thrown state lives in the cell id field, which every wall cell
   otherwise leaves at zero: unthrown the lever is at the top of its slot and
   the lamp's glass is black, thrown the lever is at the bottom and the glass
   is lit white. The conduit runs floor to ceiling a little to one side, a
   column wide by footprint like a locker upright, and a branch runs from it
   into the box at mid height. Both are near detail only, and the conduit is
   dropped past LAB_LINE_HALF for the reason given there. The box is drawn at
   every distance: a switch the player cannot pick out across a room is a
   switch they will never find. */

#define SWITCH_BOX_L 88    /* wallX extent of the box */
#define SWITCH_BOX_R 168
#define SWITCH_FRAME 8     /* frame width inside the box's edges */
#define SWITCH_SLOT_R 136  /* lever slot from the face's left edge to here */
#define SWITCH_LAMP_L 140  /* lamp and label, centred in the right panel */
#define SWITCH_LAMP_R 156
#define SWITCH_CONDUIT 184 /* face position of the conduit */

static u16 drawSwitchBox(s16 x, s16 y, s16 h, const wallhit_t* hit)
{
	s16 depth = fp2int(hit->f_wallDist);
	s16 wallx = hit->f_wallX;
	s16 boxY, boxH, rail, faceY, faceH, slotY, slotH, leverH, lampH;
	u16 thrown = mapCellId(hit->cell) == WALL_SWITCH_THROWN;

	drawConcretePanels(x, y, h, hit);

	boxH = (h >> 2) + (h >> 3);

	if(boxH < 8)
		boxH = 8;

	/* Row 80 is eye level; the box is centred on it, over the stripes. */
	boxY = 80 - (boxH >> 1);
	rail = boxH >> 3;

	if(rail < 1)
		rail = 1;

	if(depth < LAB_PANEL_NEAR)
	{
		if(hit->f_wallXHalf <= LAB_LINE_HALF &&
			wallSpans(hit, SWITCH_CONDUIT, SWITCH_CONDUIT + 1))
		{
			bmFillRect4(x, y, h, blackBm);

			return TRUE;
		}

		if(wallx >= SWITCH_BOX_R && wallx < SWITCH_CONDUIT)
			bmFillRect4(x, boxY + (boxH >> 1) - (rail >> 1), rail, blackBm);
	}

	if(wallx < SWITCH_BOX_L || wallx >= SWITCH_BOX_R)
		return TRUE;

	bmFillRect4(x, boxY, boxH, blackBm);

	if(wallx < SWITCH_BOX_L + SWITCH_FRAME || wallx >= SWITCH_BOX_R - SWITCH_FRAME)
		return TRUE;

	faceY = boxY + rail;
	faceH = boxH - rail - rail;
	slotY = faceY + rail;
	slotH = faceH - rail - rail;

	bmClearRect4(x, faceY, faceH, blackBm);
	bmClearRect4(x, faceY, faceH, greyBm);

	if(wallx < SWITCH_SLOT_R)
	{
		/* Lever slot: black, with the pale lever block at one end and its
		   handle a line across the block. */
		leverH = slotH >> 2;

		if(leverH < 3)
			leverH = 3;

		bmFillRect4(x, slotY, slotH, blackBm);

		if(thrown)
			slotY += slotH - leverH;

		bmClearRect4(x, slotY, leverH, blackBm);
		bmFillRect4(x, slotY + (leverH >> 1), 1, blackBm);

		return TRUE;
	}

	/* Lamp panel: grey, the lamp in its white bezel at the top and the label
	   below. */
	bmFillRect4(x, faceY, faceH, greyBm);

	if(wallx < SWITCH_LAMP_L || wallx >= SWITCH_LAMP_R)
		return TRUE;

	lampH = boxH >> 2;

	if(lampH < 3)
		lampH = 3;

	bmClearRect4(x, slotY, lampH, greyBm);

	if(!thrown)
		bmFillRect4(x, slotY + 1, lampH - 2, blackBm);

	bmClearRect4(x, slotY + lampH + (slotH >> 2), rail, greyBm);

	return TRUE;
}

u16 drawWallLab(u16 x, wallhit_t* hit)
{
	s16 y;
	s16 h = hit->wallHeight;
	u16 wallType = GET_CELL_TYPE_ID(hit->cell);

	y = 80 - (h >> 1);

	switch(wallType)
	{
		case WALL_TYPE_SOLID:
			return drawConcretePanels(x, y, h, hit);
		case WALL_TYPE_SHOOTABLE:
			return drawVentPanel(x, y, h, hit);
		case WALL_TYPE_ARCH:
			return drawArchway(x, y, h, hit);
		case WALL_TYPE_UNLOCKED_DOOR:
			return drawAirlockDoor(x, y, h, hit);
		case WALL_TYPE_LOCKED_DOOR:
			return drawLockedAirlockDoor(x, y, h, hit);
		case WALL_TYPE_DARK:
			return drawLockers(x, y, h, hit);
		case WALL_TYPE_BARS:
			return drawGlazedPartition(x, y, h, hit);
		case WALL_TYPE_WINDOW:
			return drawObservationWindow(x, y, h, hit);
		case WALL_TYPE_VOID:
			return drawVoid(x);
		case WALL_TYPE_SIGN:
			return drawSignboard(x, y, h, hit);
		case WALL_TYPE_LIGHT:
			return drawLitPanels(x, y, h, hit);
		case WALL_TYPE_PIPES:
			return drawConduitRuns(x, y, h, hit);
		case WALL_TYPE_SWITCH:
			return drawSwitchBox(x, y, h, hit);
		case WALL_TYPE_DADO:
			return drawTiledDado(x, y, h, hit);
		case WALL_TYPE_LOW:
			return drawBench(x, y, h, hit);
		case WALL_TYPE_PILLAR:
			return drawStrut(x, y, h, hit);
	}

	return TRUE;
}
