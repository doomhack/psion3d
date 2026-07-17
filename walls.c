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

	if(depth >= 6)
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

	if(fp2int(hit->f_wallDist) < 6)
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

static u16 drawWallD(s16 x, s16 y, s16 w, s16 h, const wallhit_t* hit)
{
	s16 doorgap, dleft, dright;
	s16 dist = hit->f_wallDist >> 4;
	s16 wallx = (hit->f_wallX >> 4);

	if(dist > 16)
		doorgap = 0;
	else
		doorgap = 16 - dist;

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

static u16 drawWallP(s16 x, s16 y, s16 w, s16 h, const wallhit_t* hit)
{
	bmFillRect4(x, y, h, blackBm);

	if(fp2int(hit->f_wallDist) < 6)
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

u16 drawWallDefault(u16 x, wallhit_t* hit)
{
	u16 updateZ = TRUE;
	s16 y;
	s16 w = 4;
	s16 h = hit->wallHeight;
	u16 wall_type = GET_CELL_TYPE_ID(hit->cell);

	y = 80 - (h >> 1);

	switch(wall_type)
	{
		case WALL_TYPE_WINDOW:
			hit->side = 0;
			break;
		case WALL_TYPE_SECRET:
			y += 2;
			h -= 4;
			hit->side = 1 - hit->side;
			break;
	}

	switch(wall_type)
	{
		case WALL_TYPE_BRICK:
		case WALL_TYPE_SECRET:
			updateZ = drawWallX(x, y, w, h, hit);
			break;
		case WALL_TYPE_ARCH:
			updateZ = drawWallA(x, y, w, h, hit);
			break;
		case WALL_TYPE_UNLOCKED_DOOR:
			updateZ = drawWallD(x, y, w, h, hit);
			break;
		case WALL_TYPE_DARK:
			updateZ = drawWallP(x, y, w, h, hit);
			break;
		case WALL_TYPE_BARS:
			updateZ = drawWallB(x, y, w, h, hit);
			break;
		case WALL_TYPE_WINDOW:
			updateZ = drawWallW(x, y, w, h, hit);
			break;
		case WALL_TYPE_VOID:
			updateZ = drawWallV(x, y, w, h, hit);
			break;
	}

	return updateZ;
}
