#include "psion3d.h"
#include "bitmap.h"
#include "game_map.h"
#include "walls.h"

/* Lab wall set. All detail is made from solid, clear, or dithered spans. */

static void labDepthWall(s16 x, s16 y, s16 h, const wallhit_t* hit)
{
	s16 depth = fp2int(hit->f_wallDist);

	if(depth >= WALL_DETAIL_DEPTH)
	{
		bmFillRect4(x, y, h, blackBm);
		return;
	}

	bmFillPattern4(x, y, h, blackBm);

	if(depth >= 4 || hit->side)
		bmFillRect4(x, y, h, greyBm);
	else
		bmClearRect4(x, y, h, greyBm);
}

static void panelSeams(s16 x, s16 y, s16 h, const wallhit_t* hit)
{
	s16 wallx = hit->f_wallX;
	s16 rail = h >> 4;

	if(rail < 1)
		rail = 1;

	/* Sealed horizontal joints at one quarter and three quarters. */
	bmFillRect4(x, y + (h >> 2), rail, blackBm);
	bmFillRect4(x, y + h - (h >> 2) - rail, rail, blackBm);

	/* Narrow vertical joins between prefabricated panels. */
	if((wallx >= 62 && wallx < 70) ||
		(wallx >= 190 && wallx < 198))
		bmFillRect4(x, y, h, blackBm);
}

static u16 drawBrickPanels(s16 x, s16 y, s16 h, const wallhit_t* hit)
{
	s16 depth = fp2int(hit->f_wallDist);
	/* y is already 80 - (wallHeight >> 1) for this wall type. */
	s16 bottom = 80 + (hit->wallHeight >> 1);
	s16 wallheight8 = (hit->wallHeight >> 3);
	s16 wallheight16 = wallheight8 >> 1;

	if(depth >= WALL_DETAIL_DEPTH)
	{
		bmFillRect4(x, y, h, blackBm);

		return TRUE;
	}
	else if(depth >= 4 || hit->side)
	{
		bmFillRect4(x, y, h, greyBm);
	}
	else
	{
		bmFillPattern4(x, y, h, greyBm);
	}

	/* Band at the horizon first: it is always on screen and the widest, so it
	   is the one to keep when BRICK_BAND_COUNT is reduced. */
#if BRICK_BAND_COUNT >= 1
	bmFillPattern4(x, 80-wallheight8, wallheight8, blackBm);
#endif
#if BRICK_BAND_COUNT >= 2
	bmFillPattern4(x, y + wallheight8, wallheight8, blackBm);
#endif
#if BRICK_BAND_COUNT >= 3
	bmFillPattern4(x, bottom - wallheight16, wallheight16, blackBm);
#endif

	return TRUE;
}

static u16 drawShootablePanel(s16 x, s16 y, s16 h, const wallhit_t* hit)
{
	labDepthWall(x, y, h, hit);

	if(fp2int(hit->f_wallDist) < WALL_DETAIL_DEPTH)
		panelSeams(x, y, h, hit);

	return TRUE;
}

static u16 drawLabPassage(s16 x, s16 y, s16 h, const wallhit_t* hit)
{
	s16 wallx = hit->f_wallX;
	s16 capHeight = h >> 2;

	if(wallx > 32 && wallx < 224)
	{
		bmFillRect4(x, y, capHeight, greyBm);
		bmFillPattern4(x, y, capHeight, blackBm);

		return FALSE;
	}

	bmFillRect4(x, y, h, greyBm);
	bmFillPattern4(x, y, h, blackBm);

	return TRUE;
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

	if(dist >= 16)
		return drawAirlock(x, y, h, hit, 0);

	doorgap = 16 - dist;
	halfgap = (doorgap << 2) + (doorgap << 1);

	if(halfgap > DOOR_TRAVEL)
		halfgap = DOOR_TRAVEL;

	return drawAirlock(x, y, h, hit, halfgap);
}

static u16 drawHazardBulkhead(s16 x, s16 y, s16 h, const wallhit_t* hit)
{
	s16 wallx = hit->f_wallX >> 4;
	s16 bandY = y + (h >> 2);
	s16 bandH = h >> 2;
	s16 step;

	bmFillRect4(x, y, h, blackBm);
	bmFillRect4(x, bandY, bandH, greyBm);

	/* Chunky stepped warning stripe, made from vertical rectangles. */
	step = (wallx + ((bandY >> 2) & 3)) & 3;
	if(step < 2)
		bmClearRect4(x, bandY, bandH, blackBm);

	return TRUE;
}

/* Glazed partition. A black frame divides the wall into three bays, each a lit
   transom over dithered glass over a solid spandrel. The glass writes the black
   plane only, so the wall behind keeps its grey and still shows through, and no
   span reaches outside a panel it paints over. The cell stays non occluding. */

#define BARS_PITCH 80  /* one bay plus one mullion, in wallX units */
#define BARS_BAY 64    /* glazed width of a bay, leaving 16 for the mullion */

static u16 drawServiceGrille(s16 x, s16 y, s16 h, const wallhit_t* hit)
{
	s16 bx = hit->f_wallX - (BARS_PITCH - BARS_BAY);
	s16 rail = h >> 6;
	s16 transH = h >> 2;
	s16 glassY = y + transH;
	s16 spandY = y + h - transH;
	s16 lightH;

	if(rail < 1)
		rail = 1;

	/* Fold the three bays onto one pitch, so one range test covers the outer
	   frame and both mullions. */
	if(bx >= BARS_PITCH + BARS_PITCH)
		bx -= BARS_PITCH + BARS_PITCH;
	else if(bx >= BARS_PITCH)
		bx -= BARS_PITCH;

	if(bx < 0 || bx >= BARS_BAY)
	{
		bmFillRect4(x, y, h, blackBm);

		return FALSE;
	}

	/* Transom, with a pale light panel skewed across it. The panel only has to
	   clear the grey plane, the black one having gone with the transom. */
	bmClearRect4(x, y, transH, blackBm);
	bmFillRect4(x, y, transH, greyBm);
	bmFillRect4(x, y, rail, blackBm);
	bmFillRect4(x, glassY - rail, rail, blackBm);

	if(bx >= 12 && bx < 52)
	{
		lightH = transH >> 2;

		bmClearRect4(x, y + lightH + ((BARS_BAY - bx) >> 4), lightH, greyBm);
	}

	bmFillPattern4(x, glassY, spandY - glassY, blackBm);

	bmClearRect4(x, spandY, transH, blackBm);
	bmFillRect4(x, spandY, transH, greyBm);
	bmFillRect4(x, spandY, rail, blackBm);
	bmFillRect4(x, y + h - rail, rail, blackBm);

	return FALSE;
}

/* Observation window. Three nested frame rings - black, grey, then a thin black
   lip - set into a pale wall, with dithered glass inside and a skirting band
   below. Like the grille the glass writes the black plane only, so the wall
   behind shows through it and no clear reaches across it. */

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
	s16 skirtY = y + h - (h >> 3);
	s16 glintY, glintH;

	if(fx < WIN_INNER)
	{
		/* No glass in this column, so it is opaque and the rings can simply be
		   overdrawn onto a cleared column, outermost first. */
		bmClearRect4(x, y, h, blackBm);
		bmClearRect4(x, y, h, greyBm);

		if(fx >= WIN_MARGIN)
			bmFillRect4(x, t1, b1 - t1, blackBm);

		if(fx >= WIN_OUTER)
		{
			bmClearRect4(x, t2, b2 - t2, blackBm);
			bmFillRect4(x, t2, b2 - t2, greyBm);
		}

		if(fx >= WIN_MID)
			bmFillRect4(x, t3, b3 - t3, blackBm);

		bmFillRect4(x, skirtY, h >> 4, greyBm);

		return TRUE;
	}

	/* Head: pale wall, then the three frame bands, each drawn as its own span so
	   that nothing has to be cleared back off again. */
	bmClearRect4(x, y, t4 - y, blackBm);
	bmClearRect4(x, y, t1 - y, greyBm);
	bmFillRect4(x, t1, t2 - t1, blackBm);
	bmFillRect4(x, t2, t3 - t2, greyBm);
	bmFillRect4(x, t3, t4 - t3, blackBm);

	bmFillPattern4(x, t4, b4 - t4, blackBm);

	/* A glint raked across the glass. It hides the view, so it clears both
	   planes, and it is clamped because the span primitives clip to the screen
	   rather than to the pane. */
	if(wallx >= 70 && wallx < 150)
	{
		glintH = (b4 - t4) >> 3;
		glintY = t4 + glintH + ((wallx - 70) >> 2);

		if(glintY + glintH > b4)
			glintY = b4 - glintH;

		bmClearRect4(x, glintY, glintH, blackBm);
		bmClearRect4(x, glintY, glintH, greyBm);
	}

	/* Sill: the three bands again, then the pale wall and its skirting. */
	bmClearRect4(x, b4, y + h - b4, blackBm);
	bmClearRect4(x, b1, y + h - b1, greyBm);
	bmFillRect4(x, b4, b3 - b4, blackBm);
	bmFillRect4(x, b3, b2 - b3, greyBm);
	bmFillRect4(x, b2, b1 - b2, blackBm);
	bmFillRect4(x, skirtY, h >> 4, greyBm);

	return FALSE;
}

static u16 drawLabVoid(s16 x)
{
	bmFillRect4(x, 80, 1, blackBm);
	return FALSE;
}

/* Wall mounted display. The same role as the default style painted board, but
   here the face is lit: it clears both planes, which leaves the background
   shade - the brightest the display has, and the only true highlight in the
   palette. */
static u16 drawLabDisplay(s16 x, s16 y, s16 h, const wallhit_t* hit)
{
	s16 wallx = hit->f_wallX;
	s16 boardY, boardH, rail;

	labDepthWall(x, y, h, hit);

	if(wallx < 64 || wallx >= 192)
	{
		if(fp2int(hit->f_wallDist) < WALL_DETAIL_DEPTH)
			panelSeams(x, y, h, hit);

		return TRUE;
	}

	boardY = y + (h >> 2);
	boardH = h >> 1;
	rail = h >> 5;

	if(rail < 1)
		rail = 1;

	bmFillRect4(x, boardY, boardH, blackBm);

	if(wallx < 72 || wallx >= 184)
		return TRUE;

	bmClearRect4(x, boardY + rail, boardH - rail - rail, blackBm);
	bmClearRect4(x, boardY + rail, boardH - rail - rail, greyBm);

	/* Lines of readout across the screen. */
	if(fp2int(hit->f_wallDist) < WALL_DETAIL_DEPTH)
	{
		bmFillRect4(x, boardY + (boardH >> 2), rail, blackBm);
		bmFillRect4(x, boardY + (boardH >> 1), rail, blackBm);
	}

	return TRUE;
}

/* Strip light on a dark bulkhead. It runs the full width of the face, which is
   the expensive shape, so the spill below it is held back to near walls: three
   spans a column out at distance, five up close. */
static u16 drawLabStripLight(s16 x, s16 y, s16 h, const wallhit_t* hit)
{
	s16 lampY = y + (h >> 3);
	s16 lampH = h >> 4;

	bmFillRect4(x, y, h, blackBm);

	bmClearRect4(x, lampY, lampH, blackBm);
	bmClearRect4(x, lampY, lampH, greyBm);

	if(fp2int(hit->f_wallDist) < WALL_DETAIL_DEPTH)
	{
		bmClearRect4(x, lampY + lampH, lampH, blackBm);
		bmFillRect4(x, lampY + lampH, lampH, greyBm);
	}

	return TRUE;
}

/* Conduit runs. Vertical detail, so a column either carries a run or it does
   not - one extra span at most, none on the wall between. The brackets sit
   inside the run own columns, so they cost nothing anywhere else. */
static u16 drawLabConduit(s16 x, s16 y, s16 h, const wallhit_t* hit)
{
	s16 wallx = hit->f_wallX;
	s16 band;

	labDepthWall(x, y, h, hit);

	if((wallx >= 32 && wallx < 48) ||
		(wallx >= 104 && wallx < 120) ||
		(wallx >= 192 && wallx < 208))
	{
		bmFillRect4(x, y, h, blackBm);

		if(fp2int(hit->f_wallDist) >= WALL_DETAIL_DEPTH)
			return TRUE;

		band = h >> 6;

		if(band < 1)
			band = 1;

		/* Brackets. The black plane sits over the grey, so a pale band on a
		   black run has to clear black before it will show at all. */
		bmClearRect4(x, y + (h >> 2), band, blackBm);
		bmFillRect4(x, y + (h >> 2), band, greyBm);
		bmClearRect4(x, y + h - (h >> 2), band, blackBm);
		bmFillRect4(x, y + h - (h >> 2), band, greyBm);

		return TRUE;
	}

	/* Lit edge down the side of each run, so it reads as round. */
	if((wallx >= 48 && wallx < 56) ||
		(wallx >= 120 && wallx < 128) ||
		(wallx >= 208 && wallx < 216))
	{
		bmClearRect4(x, y, h, blackBm);
		bmFillRect4(x, y, h, greyBm);
	}

	return TRUE;
}

/* Equipment racking. The shelves run the full width of the face, which is the
   costly shape - three such bands on the brick wall measured 5.6ms a frame - so
   this is held to two, and it is a feature wall rather than a corridor filler. */
static u16 drawLabRack(s16 x, s16 y, s16 h, const wallhit_t* hit)
{
	s16 wallx = hit->f_wallX;
	s16 upperY = y + (h >> 2);
	s16 lowerY = y + h - (h >> 2);
	s16 rail = h >> 5;
	s16 lamp;

	labDepthWall(x, y, h, hit);

	if(fp2int(hit->f_wallDist) >= WALL_DETAIL_DEPTH)
		return TRUE;

	if(rail < 1)
		rail = 1;

	/* Frame upright first: vertical, so the bays either side pay nothing. */
	if(wallx >= 120 && wallx < 136)
	{
		bmFillRect4(x, y, h, blackBm);

		return TRUE;
	}

	bmFillRect4(x, upperY, rail, blackBm);
	bmFillRect4(x, lowerY, rail, blackBm);

	/* Indicator lamps on the gear in every other bay. */
	if((wallx >> 5) & 1)
	{
		lamp = h >> 5;

		if(lamp < 1)
			lamp = 1;

		bmClearRect4(x, upperY - (h >> 4), lamp, blackBm);
		bmClearRect4(x, upperY - (h >> 4), lamp, greyBm);
	}

	return TRUE;
}

/* A bench or partition low enough to see over. The cell is not solid, so the
   ray ran on and what stands behind it went into this column first; the bench
   is opaque, so it clears the black plane back off over its own half.

   It returns FALSE, leaving the column non occluding. The depth buffer holds
   one distance per column and cannot say solid-below-row-100, so the choice is
   between a sprite behind the bench drawing over it and that sprite
   disappearing in the open air above it. The first is the lesser error and is
   what BARS and WINDOW already do here. */
static u16 drawLabBench(s16 x, s16 y, s16 h, const wallhit_t* hit)
{
	s16 top = y + (h >> 1);
	s16 lowH = h - (h >> 1);
	s16 coping = h >> 5;

	if(coping < 1)
		coping = 1;

	bmClearRect4(x, top, lowH, blackBm);
	bmFillRect4(x, top, lowH, greyBm);
	bmFillRect4(x, top, coping, blackBm);

	/* Kick plate under the worktop. */
	if(fp2int(hit->f_wallDist) < WALL_DETAIL_DEPTH)
		bmFillPattern4(x, y + h - (lowH >> 2), lowH >> 2, blackBm);

	return FALSE;
}

/* A structural strut standing in the cell. Not solid, so the columns either
   side of it keep whatever the ray found behind. The strut is opaque and writes
   both planes outright rather than going through labDepthWall, whose dither
   would let the wall behind show through it. */
static u16 drawLabStrut(s16 x, s16 y, s16 h, const wallhit_t* hit)
{
	s16 wallx = hit->f_wallX;
	s16 band, flange;

	if(wallx < 96 || wallx >= 160)
		return FALSE;

	bmClearRect4(x, y, h, blackBm);
	bmFillRect4(x, y, h, greyBm);

	if(fp2int(hit->f_wallDist) >= WALL_DETAIL_DEPTH)
		return TRUE;

	band = h >> 4;

	if(band < 1)
		band = 1;

	flange = band >> 1;

	if(flange < 1)
		flange = 1;

	/* Cap, base, and two bolted flanges between them. */
	bmFillRect4(x, y, band, blackBm);
	bmFillRect4(x, y + h - band, band, blackBm);
	bmFillRect4(x, y + (h >> 2), flange, blackBm);
	bmFillRect4(x, y + h - (h >> 2), flange, blackBm);

	/* A lit edge down one side, so the strut reads as round rather than flat. */
	if(wallx >= 144)
		bmClearRect4(x, y + band, h - band - band, greyBm);

	return TRUE;
}

/* Control panel set into the wall. Thrown state lives in the cell id field,
   which every wall cell otherwise leaves at zero, and shows as a lit panel
   rather than a dark one. Drawn at every distance: a switch the player cannot
   pick out across a room is a switch they will never find. */
static u16 drawLabControlPanel(s16 x, s16 y, s16 h, const wallhit_t* hit)
{
	s16 wallx = hit->f_wallX;
	s16 plateY, plateH, rail;

	labDepthWall(x, y, h, hit);

	if(wallx < 104 || wallx >= 152)
	{
		if(fp2int(hit->f_wallDist) < WALL_DETAIL_DEPTH)
			panelSeams(x, y, h, hit);

		return TRUE;
	}

	/* Row 80 is eye level, so the plate sits a little under it. */
	plateH = h >> 3;

	if(plateH < 3)
		plateH = 3;

	plateY = 80 - (plateH >> 1) + (h >> 4);
	rail = plateH >> 2;

	if(rail < 1)
		rail = 1;

	bmFillRect4(x, plateY, plateH, blackBm);

	if(wallx < 112 || wallx >= 144)
		return TRUE;

	bmClearRect4(x, plateY + rail, plateH - rail - rail, blackBm);

	if(mapCellId(hit->cell) == WALL_SWITCH_THROWN)
		bmClearRect4(x, plateY + rail, plateH - rail - rail, greyBm);
	else
		bmFillRect4(x, plateY + rail, plateH - rail - rail, greyBm);

	return TRUE;
}

u16 drawWallLab(u16 x, wallhit_t* hit)
{
	s16 y;
	s16 h = hit->wallHeight;
	u16 wallType = GET_CELL_TYPE_ID(hit->cell);

	y = 80 - (h >> 1);

	if(wallType == WALL_TYPE_SHOOTABLE)
	{
		y += 2;
		h -= 4;
		hit->side = 1 - hit->side;
	}

	switch(wallType)
	{
		case WALL_TYPE_SOLID:
			return drawBrickPanels(x, y, h, hit);
		case WALL_TYPE_SHOOTABLE:
			return drawShootablePanel(x, y, h, hit);
		case WALL_TYPE_ARCH:
			return drawLabPassage(x, y, h, hit);
		case WALL_TYPE_UNLOCKED_DOOR:
			return drawAirlockDoor(x, y, h, hit);
		case WALL_TYPE_LOCKED_DOOR:
			return drawLockedAirlockDoor(x, y, h, hit);
		case WALL_TYPE_DARK:
			return drawHazardBulkhead(x, y, h, hit);
		case WALL_TYPE_BARS:
			return drawServiceGrille(x, y, h, hit);
		case WALL_TYPE_WINDOW:
			return drawObservationWindow(x, y, h, hit);
		case WALL_TYPE_VOID:
			return drawLabVoid(x);

		case WALL_TYPE_SIGN:
			return drawLabDisplay(x, y, h, hit);

		case WALL_TYPE_LIGHT:
			return drawLabStripLight(x, y, h, hit);

		case WALL_TYPE_PIPES:
			return drawLabConduit(x, y, h, hit);

		case WALL_TYPE_SHELF:
			return drawLabRack(x, y, h, hit);

		case WALL_TYPE_LOW:
			return drawLabBench(x, y, h, hit);

		case WALL_TYPE_PILLAR:
			return drawLabStrut(x, y, h, hit);

		case WALL_TYPE_SWITCH:
			return drawLabControlPanel(x, y, h, hit);
	}

	return TRUE;
}
