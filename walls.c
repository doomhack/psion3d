#include "psion3d.h"
#include "bitmap.h"
#include "game_map.h"
#include "walls.h"

#define MODE_FILL 0
#define MODE_CLEAR 1

wall_draw_fn drawWall;

static void brickPattern(s16 x, s16 y, s16 w, s16 h, const wallhit_t* hit, const u16 mode)
{
	s16 qheight = (hit->wallHeight >> 2);
	s16 wallx = hit->f_wallX;

	if(mode == MODE_FILL)
	{
		bmFillRect4(x, 80, 1, blackBm);
		bmFillRect4(x, 80 - qheight, 1, blackBm);
		bmFillRect4(x, 80 + qheight, 1, blackBm);
	}
	else if(mode == MODE_CLEAR)
	{
		bmClearRect4(x, 80, 1, blackBm);
		bmClearRect4(x, 80 - qheight, 1, blackBm);
		bmClearRect4(x, 80 + qheight, 1, blackBm);
	}

	if((wallx >= 64 && wallx < 72) || (wallx >= 192 && wallx < 200))
	{
		if(mode == MODE_FILL)
		{
			bmFillRect(x, y, 1, qheight, blackBm);
			bmFillRect(x, 80, 1, qheight, blackBm);
		}
		else if(mode == MODE_CLEAR)
		{
			bmClearRect(x, y, 1, qheight, blackBm);
			bmClearRect(x, 80, 1, qheight, blackBm);
		}
	}
}

static void depthWall(s16 x, s16 y, s16 w, s16 h, const wallhit_t* hit)
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

static u16 drawWallX(s16 x, s16 y, s16 w, s16 h, const wallhit_t* hit)
{
	depthWall(x, y, w, h, hit);

	if(fp2int(hit->f_wallDist) < WALL_DETAIL_DEPTH)
		brickPattern(x, y, w, h, hit, MODE_FILL);

	return TRUE;
}

static u16 drawWallA(s16 x, s16 y, s16 w, s16 h, const wallhit_t* hit)
{
	s16 wallx = hit->f_wallX;

	if(wallx > 24 && wallx < 232)
	{
		depthWall(x, y, w, hit->wallHeight >> 2, hit);
		return FALSE;
	}

	depthWall(x, y, w, h, hit);
	return TRUE;
}

/* One doorway, opened by however much the caller asks for. The unlocked door
   draws its gap from the ray distance so it opens as the player nears it; the
   locked one is the same door held shut, which is what WALL_TYPE_LOCKED_DOOR
   had no case for at all before. */
static u16 drawWallDoorGap(s16 x, s16 y, s16 w, s16 h, const wallhit_t* hit, s16 doorgap)
{
	s16 dleft, dright;
	s16 wallx = (hit->f_wallX >> 4);

	dleft = 8 - (doorgap >> 1);
	dright = 8 + (doorgap >> 1);

	if(wallx == dleft || wallx == dright)
		bmFillRect4(x, y, h, blackBm);
	else if(wallx < dleft || wallx > dright)
	{
		if((wallx < (dleft - 2) && wallx >= (dleft - 5)) ||
			(wallx > (dright + 2) && wallx <= (dright + 5)))
		{
			bmFillRect4(x, y, h, greyBm);
			bmClearRect4(x, y, (h >> 2), blackBm);
			bmClearRect4(x, y + (h >> 1), (h >> 1), blackBm);
		}
		else
		{
			bmClearRect4(x, y, h, blackBm);
			bmFillRect4(x, y, h, greyBm);
		}

		bmFillPattern4(x, y + h - (h >> 3), h >> 3, blackBm);
	}
	else
		return FALSE;

	return TRUE;
}

static u16 drawWallD(s16 x, s16 y, s16 w, s16 h, const wallhit_t* hit)
{
	s16 dist = hit->f_wallDist >> 4;

	return drawWallDoorGap(x, y, w, h, hit, (dist > 16) ? 0 : (16 - dist));
}

static u16 drawWallT(s16 x, s16 y, s16 w, s16 h, const wallhit_t* hit)
{
	return drawWallDoorGap(x, y, w, h, hit, 0);
}

static u16 drawWallP(s16 x, s16 y, s16 w, s16 h, const wallhit_t* hit)
{
	bmFillRect4(x, y, h, blackBm);

	if(fp2int(hit->f_wallDist) < WALL_DETAIL_DEPTH)
		brickPattern(x, y, w, h, hit, MODE_CLEAR);

	return TRUE;
}

static u16 drawWallB(s16 x, s16 y, s16 w, s16 h, const wallhit_t* hit)
{
	s16 top = y;
	s16 bottom = y + h;
	s16 capHeight = hit->wallHeight >> 3;

	depthWall(x, top, w, capHeight, hit);
	depthWall(x, bottom - capHeight, w, capHeight, hit);

	if((hit->f_wallX >> 3) & 1)
	{
		bmFillRect4(x, y + (h >> 3), h - (h >> 2), blackBm);
		return TRUE;
	}

	return FALSE;
}

static u16 drawWallW(s16 x, s16 y, s16 w, s16 h, const wallhit_t* hit)
{
	s16 top = y;
	s16 bottom = y + h;
	s16 capHeight = hit->wallHeight >> 2;

	bmFillRect4(x, y + (h >> 2), h - (h >> 1), greyBm);
	depthWall(x, top, w, capHeight, hit);
	depthWall(x, bottom - capHeight, w, capHeight, hit);

	return FALSE;
}

static u16 drawWallV(s16 x, s16 y, s16 w, s16 h, const wallhit_t* hit)
{
	bmFillRect4(x, 80, 1, blackBm);
	return FALSE;
}

/* A board hung on the middle of the wall - notice, sign, plaque. The frame is
   one span on the columns that carry it and the face three more, so the cost
   falls only on the middle half of the face. */
static u16 drawWallSign(s16 x, s16 y, s16 w, s16 h, const wallhit_t* hit)
{
	s16 wallx = hit->f_wallX;
	s16 boardY, boardH, rail;

	depthWall(x, y, w, h, hit);

	if(wallx < 64 || wallx >= 192)
	{
		if(fp2int(hit->f_wallDist) < WALL_DETAIL_DEPTH)
			brickPattern(x, y, w, h, hit, MODE_FILL);

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
	bmFillRect4(x, boardY + rail, boardH - rail - rail, greyBm);

	/* Two lines of lettering, suggested rather than written. */
	if(fp2int(hit->f_wallDist) < WALL_DETAIL_DEPTH)
	{
		bmFillRect4(x, boardY + (boardH >> 2), rail, blackBm);
		bmFillRect4(x, boardY + (boardH >> 1), rail, blackBm);
	}

	return TRUE;
}

/* A light burning on a dark wall. Clearing both planes leaves the background
   shade, which is the brightest the display has and the only true highlight in
   the palette - so the lamp is drawn at every distance, not just inside
   WALL_DETAIL_DEPTH. It is what makes the wall worth walking towards. */
static u16 drawWallLight(s16 x, s16 y, s16 w, s16 h, const wallhit_t* hit)
{
	s16 wallx = hit->f_wallX;
	s16 lampY, lampH;

	drawWallP(x, y, w, h, hit);

	if(wallx < 96 || wallx >= 160)
		return TRUE;

	lampH = h >> 3;
	lampY = y + (h >> 2);

	bmClearRect4(x, lampY, lampH, blackBm);
	bmClearRect4(x, lampY, lampH, greyBm);

	/* The light it throws down the wall below it. */
	bmClearRect4(x, lampY + lampH, lampH, blackBm);
	bmFillRect4(x, lampY + lampH, lampH, greyBm);

	return TRUE;
}

/* Vertical runs down the wall - pipes, cabling, creeper, timber uprights. The
   detail is vertical, so a column either is a run or it is not: one extra span
   at most, and on most columns none at all. The cheapest of the set, and the
   one to reach for when a long stretch of wall needs breaking up. */
static u16 drawWallPipes(s16 x, s16 y, s16 w, s16 h, const wallhit_t* hit)
{
	s16 wallx = hit->f_wallX;

	depthWall(x, y, w, h, hit);

	if((wallx >= 40 && wallx < 56) ||
		(wallx >= 112 && wallx < 128) ||
		(wallx >= 200 && wallx < 216))
	{
		bmFillRect4(x, y, h, blackBm);

		return TRUE;
	}

	/* A lit edge down the right of each run, so it reads as round. */
	if((wallx >= 56 && wallx < 64) ||
		(wallx >= 128 && wallx < 136) ||
		(wallx >= 216 && wallx < 224))
	{
		bmClearRect4(x, y, h, blackBm);
		bmFillRect4(x, y, h, greyBm);
	}

	return TRUE;
}

/* A furnished wall - shelving, racking, lockers, crates. The shelves run the
   full width of the face, which is the expensive shape: three such bands in the
   lab brick wall measured 5.6ms a frame. Held to two shelves and one box for
   that reason, and it is a feature wall rather than a corridor filler. */
static u16 drawWallShelf(s16 x, s16 y, s16 w, s16 h, const wallhit_t* hit)
{
	s16 wallx = hit->f_wallX;
	s16 upperY = y + (h >> 2);
	s16 lowerY = y + h - (h >> 2);
	s16 rail = h >> 5;

	depthWall(x, y, w, h, hit);

	if(fp2int(hit->f_wallDist) >= WALL_DETAIL_DEPTH)
		return TRUE;

	if(rail < 1)
		rail = 1;

	/* The upright first: vertical, so it costs the columns between it nothing. */
	if(wallx >= 120 && wallx < 136)
	{
		bmFillRect4(x, y, h, blackBm);

		return TRUE;
	}

	bmFillRect4(x, upperY, rail, blackBm);
	bmFillRect4(x, lowerY, rail, blackBm);

	/* Boxes standing on the lower shelf, in every other bay. */
	if((wallx >> 5) & 1)
		bmFillRect4(x, lowerY - (h >> 4), h >> 4, blackBm);

	return TRUE;
}

/* A wall low enough to see over - parapet, railing, counter, rubble. The cell
   is not solid, so the ray ran on and whatever stands behind it was drawn into
   this column first; the parapet is opaque, so it clears the black plane back
   off over its own half before painting.

   It returns FALSE, which leaves the column non occluding. The depth buffer is
   one distance per column and cannot say "solid below row 100", so the choice
   is between a sprite behind the parapet drawing over it and that same sprite
   vanishing entirely in the open air above it. The first is the lesser error,
   and it is what BARS and WINDOW already do. */
static u16 drawWallLow(s16 x, s16 y, s16 w, s16 h, const wallhit_t* hit)
{
	s16 top = y + (h >> 1);
	s16 lowH = h - (h >> 1);
	s16 coping = h >> 5;

	if(coping < 1)
		coping = 1;

	bmClearRect4(x, top, lowH, blackBm);
	bmFillRect4(x, top, lowH, greyBm);
	bmFillRect4(x, top, coping, blackBm);

	if(fp2int(hit->f_wallDist) < WALL_DETAIL_DEPTH)
		bmFillRect4(x, top + (lowH >> 1), coping, blackBm);

	return FALSE;
}

/* A post standing in the cell - pillar, strut, tree, machinery stack. Not
   solid, so the columns either side of it keep whatever the ray found behind.
   The post itself is opaque and both planes are written outright rather than
   through depthWall, whose dither would let the far wall show through it. */
static u16 drawWallPillar(s16 x, s16 y, s16 w, s16 h, const wallhit_t* hit)
{
	s16 wallx = hit->f_wallX;
	s16 band;

	if(wallx < 96 || wallx >= 160)
		return FALSE;

	bmClearRect4(x, y, h, blackBm);
	bmFillRect4(x, y, h, greyBm);

	if(fp2int(hit->f_wallDist) >= WALL_DETAIL_DEPTH)
		return TRUE;

	band = h >> 4;

	if(band < 1)
		band = 1;

	/* Cap and base. */
	bmFillRect4(x, y, band, blackBm);
	bmFillRect4(x, y + h - band, band, blackBm);

	/* A lit edge down one side, so the post reads as round rather than flat. */
	if(wallx >= 144)
		bmClearRect4(x, y + band, h - band - band, greyBm);

	return TRUE;
}

/* A switch set into the wall. Thrown state lives in the cell's id field, which
   every wall cell otherwise leaves at zero, and shows as a pale panel instead
   of a mid one. Drawn at every distance: a switch the player cannot pick out
   across a room is a switch they will never find. */
static u16 drawWallSwitch(s16 x, s16 y, s16 w, s16 h, const wallhit_t* hit)
{
	s16 wallx = hit->f_wallX;
	s16 plateY, plateH, rail;

	depthWall(x, y, w, h, hit);

	if(wallx < 104 || wallx >= 152)
	{
		if(fp2int(hit->f_wallDist) < WALL_DETAIL_DEPTH)
			brickPattern(x, y, w, h, hit, MODE_FILL);

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

u16 drawWallDefault(u16 x, wallhit_t* hit)
{
	s16 y;
	s16 w = 4;
	s16 h = hit->wallHeight;
	u16 wall_type = GET_CELL_TYPE_ID(hit->cell);

	y = 80 - (h >> 1);

	/* One dispatch on wall_type, with the per type adjustments folded in. */
	switch(wall_type)
	{
		case WALL_TYPE_SOLID:
			return drawWallX(x, y, w, h, hit);

		case WALL_TYPE_SHOOTABLE:
			y += 2;
			h -= 4;
			hit->side = 1 - hit->side;
			return drawWallX(x, y, w, h, hit);

		case WALL_TYPE_ARCH:
			return drawWallA(x, y, w, h, hit);

		case WALL_TYPE_UNLOCKED_DOOR:
			return drawWallD(x, y, w, h, hit);

		case WALL_TYPE_DARK:
			return drawWallP(x, y, w, h, hit);

		case WALL_TYPE_BARS:
			return drawWallB(x, y, w, h, hit);

		case WALL_TYPE_WINDOW:
			hit->side = 0;
			return drawWallW(x, y, w, h, hit);

		case WALL_TYPE_VOID:
			return drawWallV(x, y, w, h, hit);

		case WALL_TYPE_LOCKED_DOOR:
			return drawWallT(x, y, w, h, hit);

		case WALL_TYPE_SIGN:
			return drawWallSign(x, y, w, h, hit);

		case WALL_TYPE_LIGHT:
			return drawWallLight(x, y, w, h, hit);

		case WALL_TYPE_PIPES:
			return drawWallPipes(x, y, w, h, hit);

		case WALL_TYPE_SHELF:
			return drawWallShelf(x, y, w, h, hit);

		case WALL_TYPE_LOW:
			return drawWallLow(x, y, w, h, hit);

		case WALL_TYPE_PILLAR:
			return drawWallPillar(x, y, w, h, hit);

		case WALL_TYPE_SWITCH:
			return drawWallSwitch(x, y, w, h, hit);
	}

	return TRUE;
}
