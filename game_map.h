#ifndef GAME_MAP_H
#define GAME_MAP_H

#include "fp_types.h"

#define MAP_X 64
#define MAP_Y 64

extern s8 map[MAP_Y][MAP_X];

static s8 mapCell(u16 x, u16 y)
{
	if(x >= MAP_X || y >= MAP_Y )
		return 'V';
	
	return map[y][x];
}

static s8 fmapCell(f16 fx, f16 fy)
{
	u16 x = fp2int(fx);
	u16 y = fp2int(fy);
		
	return mapCell(x, y);
}

static u16 isSprite(s8 cell)
{
	return cell == 'H';
}

static u16 isMarked(s8 cell)
{
	return cell < 0;
}

static u16 canWalk(s8 cell)
{
	switch(cell)
	{
		case '0':
		case 'A':
		case 'D':
		case 'S':
			return TRUE;
	}
	
	return isMarked(cell) || isSprite(cell);
}

static void updateCell(u16 x, u16 y, s8 cell)
{
	if(x >= MAP_X || y >= MAP_Y )
		return;
	
	map[y][x] = cell;
}

static void markSprite(u16 x, u16 y)
{
	s8 c = mapCell(x, y);
	
	if(c > 0)
		updateCell(x, y, -c);
}

static void unmarkSprite(u16 x, u16 y)
{
	s8 c = mapCell(x, y);
	
	if(c < 0)
		updateCell(x, y, -c);
}

static u16 isWall(s8 cell)
{
	if(cell != '0' && !isSprite(cell) && !isMarked(cell))
		return TRUE;
	
	return FALSE;
}

static u16 isSolid(s8 cell)
{
	switch(cell)
	{
		case 'X':
		case 'P':
		case 'V':
		case 'S':
			return TRUE;
	}
	
	return FALSE;
}

#endif