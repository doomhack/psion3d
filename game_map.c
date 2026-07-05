#include "fp_types.h"
#include "game_map.h"
#include "sprite.h"
#include "debug.h"
#include "enemy.h"

#define MAP_FILE_NAME_LEN 64

u16 map[MAP_Y][MAP_X];

void loadMapData(const u8 mapId)
{
	loadSprite("sci", SPRITE_SLOT_CIV);
	loadSprite("mer", SPRITE_SLOT_MER);
	loadSprite("sgr", SPRITE_SLOT_SGR);
	loadSprite("hvy", SPRITE_SLOT_HVY);

	loadSprite("ppk", 5);
}

u16 getCellEncoding(u16 x, u16 y, s8 cell)
{
	switch (cell)
	{
		case '0': //Open space
			return (MAP_MASK_WALK);

		//Walls
		case 'X': //Brick wall
			return (MAP_MASK_WALL | MAP_MASK_SOLID | SET_CELL_TYPE_ID(WALL_TYPE_BRICK));

		case 'P': //Dark brick wall
			return (MAP_MASK_WALL | MAP_MASK_SOLID | SET_CELL_TYPE_ID(WALL_TYPE_DARK));

		case 'W': //Window
			return (MAP_MASK_WALL | SET_CELL_TYPE_ID(WALL_TYPE_WINDOW));

		case 'A': //Archway
			return (MAP_MASK_WALL | MAP_MASK_WALK | SET_CELL_TYPE_ID(WALL_TYPE_ARCH));

		case 'D': //Unlocked Door
			return (MAP_MASK_WALL | MAP_MASK_WALK | SET_CELL_TYPE_ID(WALL_TYPE_UNLOCKED_DOOR));

		case 'S': //Secret wall
			return (MAP_MASK_WALL | MAP_MASK_SOLID | MAP_MASK_WALK | SET_CELL_TYPE_ID(WALL_TYPE_SECRET));

		case 'B': //Iron Bars
			return (MAP_MASK_WALL | SET_CELL_TYPE_ID(WALL_TYPE_BARS));

		case 'V': //The void
			return (MAP_MASK_WALL | MAP_MASK_SOLID | SET_CELL_TYPE_ID(WALL_TYPE_VOID));

		case 'C':
		case 'E':
		case 'F':
		case 'G':
			return getEnemyCell(x, y, cell);
	}

	//Unknown type. Return void.
	return (MAP_MASK_WALL | MAP_MASK_SOLID | SET_CELL_TYPE_ID(7));
}

u16 loadMap(const u8 mapId)
{
	TEXT fileName[MAP_FILE_NAME_LEN];
	VOID* fileHandle;
	u16 x = 0;
	u16 y = 0;
	INT bytesRead;
	s8 c;

	resetEnemy();

	p_atos(&fileName[0], "LOC::M:\\IMG\\MAP\\map%d.map", mapId);

	if(p_open(&fileHandle, &fileName[0], P_FOPEN | P_FSTREAM) != 0)
		return FALSE;

	while(TRUE)
	{
		bytesRead = p_read(fileHandle, &c, 1);

		if(bytesRead == E_FILE_EOF)
			break;

		if(bytesRead != 1)
		{
			p_close(fileHandle);
			return FALSE;
		}

		if(c == '\r' || c == '\n')
			continue;

		if(y >= MAP_Y)
		{
			p_close(fileHandle);
			return FALSE;
		}

		map[y][x] = getCellEncoding(x, y, c);
		x++;

		if(x >= MAP_X)
		{
			x = 0;
			y++;
		}
	}

	p_close(fileHandle);

	if(y != MAP_Y || x != 0)
	{
		return FALSE;
	}


	loadMapData(mapId);

	return TRUE;
}
