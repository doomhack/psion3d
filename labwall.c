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

static u16 drawSecretPanel(s16 x, s16 y, s16 h, const wallhit_t* hit)
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

static u16 drawServiceGrille(s16 x, s16 y, s16 h, const wallhit_t* hit)
{
	s16 capHeight = h >> 3;
	s16 wallx = (hit->f_wallX >> 3);
	s16 bottom = y + hit->wallHeight;
	s16 quarter = h >> 2;
	s16 bottomCapY = bottom - quarter;

	/* Grille bar columns end up filled black over the full height, which covers
	   every black plane span the cap detail would have drawn. Only the grey
	   plane writes still matter, so skip the rest. */
	if(wallx < 2 || wallx > 29 || wallx == 10 || wallx == 20)
	{
		bmFillRect4(x, y, capHeight, greyBm);
		bmFillRect4(x, bottomCapY, quarter, greyBm);
		bmFillRect4(x, y, h, blackBm);

		return FALSE;
	}

	//Top cap
	bmClearRect4(x, y, capHeight, blackBm);
	bmFillRect4(x, y, capHeight, greyBm);

	bmFillRect4(x, y, capHeight >> 2, blackBm);
	bmFillRect4(x, y + capHeight, capHeight >> 2, blackBm);

	bmFillPattern4(x, y + capHeight, h >> 1, blackBm);

	//Bottom cap
	bmClearRect4(x, bottomCapY, quarter, blackBm);
	bmFillRect4(x, bottomCapY, quarter, greyBm);

	bmFillRect4(x, bottomCapY, capHeight >> 3, blackBm);
	bmFillRect4(x, bottom - (capHeight >> 3), capHeight >> 3, blackBm);

	return FALSE;
}

static u16 drawObservationWindow(s16 x, s16 y, s16 h, const wallhit_t* hit)
{
	s16 frameHeight = h >> 3;
	s16 glassY = y + (h >> 2);
	s16 glassH = h >> 1;
	s16 wallx = hit->f_wallX;

	/* Dithered glass, heavy sill/header, and quarter-width mullions. */
	bmFillPattern4(x, glassY, glassH, greyBm);
	labDepthWall(x, y, frameHeight, hit);
	labDepthWall(x, y + h - frameHeight, frameHeight, hit);

	if((wallx >= 124 && wallx < 132) || wallx < 8 || wallx >= 248)
	{
		bmFillRect4(x, glassY, glassH, blackBm);
		return TRUE;
	}

	return FALSE;
}

static u16 drawLabVoid(s16 x)
{
	bmFillRect4(x, 80, 1, blackBm);
	return FALSE;
}

u16 drawWallLab(u16 x, wallhit_t* hit)
{
	s16 y;
	s16 h = hit->wallHeight;
	u16 wallType = GET_CELL_TYPE_ID(hit->cell);

	y = 80 - (h >> 1);

	if(wallType == WALL_TYPE_WINDOW)
		hit->side = 0;
	else if(wallType == WALL_TYPE_SECRET)
	{
		y += 2;
		h -= 4;
		hit->side = 1 - hit->side;
	}

	switch(wallType)
	{
		case WALL_TYPE_BRICK:
			return drawBrickPanels(x, y, h, hit);
		case WALL_TYPE_SECRET:
			return drawSecretPanel(x, y, h, hit);
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
	}

	return TRUE;
}
