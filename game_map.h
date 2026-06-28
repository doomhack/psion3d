#ifndef GAME_MAP_H
#define GAME_MAP_H

#include "fp_types.h"

#define MAP_X 64
#define MAP_Y 64

#define MAP_BIT_MARKED 15
#define MAP_BIT_SOLID 14
#define MAP_BIT_WALL 13
#define MAP_BIT_SPRITE 12
#define MAP_BIT_ENEMY 11
#define MAP_BIT_WALK 10

#define MAP_MASK_MARKED ((u16)0x8000)
#define MAP_MASK_SOLID ((u16)0x4000)
#define MAP_MASK_WALL ((u16)0x2000)
#define MAP_MASK_SPRITE ((u16)0x1000)
#define MAP_MASK_ENEMY ((u16)0x0800)
#define MAP_MASK_WALK ((u16)0x0400)

#define MAP_BLOCK_TYPE_MASK 0x3C0
#define MAP_BLOCK_ID_MASK 0x3F

#define MAP_BLOCK_TYPE_SHIFT 6

#define SET_CELL_TYPE_ID(x) ((u16)((x) << MAP_BLOCK_TYPE_SHIFT))
#define GET_CELL_TYPE_ID(x) (((x) & MAP_BLOCK_TYPE_MASK) >> MAP_BLOCK_TYPE_SHIFT)

#define WALL_TYPE_BRICK 0
#define WALL_TYPE_DARK 1
#define WALL_TYPE_WINDOW 2
#define WALL_TYPE_ARCH 3
#define WALL_TYPE_UNLOCKED_DOOR 4
#define WALL_TYPE_SECRET 5
#define WALL_TYPE_BARS 6
#define WALL_TYPE_VOID 7

#define ENEMY_TYPE_CIV 0
#define ENEMY_TYPE_MER 1
#define ENEMY_TYPE_SGR 2
#define ENEMY_TYPE_HVY 3

extern u16 map[MAP_Y][MAP_X];

void loadMapData(const u8 mapId);
u16 loadMap(const u8 mapId);

static u16 mapCell(const u16 x, const u16 y)
{
	if(x >= MAP_X || y >= MAP_Y )
		return (MAP_MASK_WALL | MAP_MASK_SOLID | SET_CELL_TYPE_ID(WALL_TYPE_VOID));
	
	return map[y][x];
}

static u16 fmapCell(const f16 fx, const f16 fy)
{
	u16 x = fp2int(fx);
	u16 y = fp2int(fy);
		
	return mapCell(x, y);
}

static u16 isSprite(const u16 cell)
{
	return (cell & MAP_MASK_SPRITE);
}

static u16 isEnemy(const u16 cell)
{
	return (cell & MAP_MASK_ENEMY);
}

static u8 mapCellType(const u16 cell)
{
	return ((cell & MAP_BLOCK_TYPE_MASK) >> MAP_BLOCK_TYPE_SHIFT);
}

static u8 mapCellId(const u16 cell)
{
	return (cell & MAP_BLOCK_ID_MASK);
}

static u16 isMarked(const u16 cell)
{
	return (cell & MAP_MASK_MARKED);
}

static u16 canWalk(const u16 cell)
{
	return (cell & MAP_MASK_WALK);
}

static void updateCell(const u16 x, const u16 y, const u16 cell)
{
	if(x >= MAP_X || y >= MAP_Y )
		return;
	
	map[y][x] = cell;
}

static void markSprite(const u16 x, const u16 y)
{
	u16 c = mapCell(x, y);
	
	if((c & MAP_MASK_MARKED) == 0)
		updateCell(x, y, (c | MAP_MASK_MARKED));
}

static void unmarkSprite(const u16 x, const u16 y)
{
	u16 c = mapCell(x, y);
	
	if(c & MAP_MASK_MARKED)
		updateCell(x, y, c & (~MAP_MASK_MARKED));
}

static u16 isWall(const u16 cell)
{
	return cell & MAP_MASK_WALL;
}

static u16 isSolid(const u16 cell)
{
	return cell & MAP_MASK_SOLID;
}

#endif
