#include "fp_types.h"
#include "game_map.h"
#include "sprite.h"
#include "debug.h"

#define MAP_FILE_NAME_LEN 32

s8 map[MAP_Y][MAP_X];

void loadMapData(const u8 mapId)
{
	if(mapId == 1)
		loadSprite("h", 0);
}

u16 loadMap(const u8 mapId)
{
	TEXT fileName[MAP_FILE_NAME_LEN];
	VOID* fileHandle;
	u16 x = 0;
	u16 y = 0;
	INT bytesRead;
	s8 c;

	p_atos(&fileName[0], "LOC::M:\\IMG\\map%d.map", mapId);

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

		map[y][x] = c;
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
