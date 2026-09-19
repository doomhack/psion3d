#include "automap.h"
#include "bitmap.h"
#include "game_map.h"
#include "player.h"
#include "fp_math.h"

/*  What each cell draws, from its flags rather than its type, so a wall
    style's meaning carries over: solid is black, a wall the ray sees through
    (window, bars, grille, parapet, pillar) is grey, a wall that can be walked
    through (arch, open door) is a grey ring, a locked door is grey with a
    black core. Open floor, and the sprites standing on it, draw nothing.

    The whole level is shown. The design has no unexplored state; when it
    wants one, isMarked() on the cell is the test to add here. */
static void drawCell(const s16 x, const s16 y, const u16 cell)
{
	if(!isWall(cell))
		return;

	if(isSolid(cell))
	{
		bmFillRect4(x, y, 4, blackBm);
		return;
	}

	if(canWalk(cell))
	{
		bmDrawRect(x, y, 4, 4, greyBm);
		return;
	}

	bmFillRect4(x, y, 4, greyBm);

	if(mapCellType(cell) == WALL_TYPE_LOCKED_DOOR)
		bmFillRect((s16)(x + 1), (s16)(y + 1), 2, 2, blackBm);
}

/*  The player as a small arrow along their facing: a 3 line triangle with
    the tip five pixels ahead of the cell centre and the base three behind. */
static void drawPlayer(const u8 topRow)
{
	const f16 f_cos = fpcos(player.pos.angle);
	const f16 f_sin = fpsin(player.pos.angle);
	s16 cx, cy;
	s16 tipX, tipY, baseX, baseY, leftX, leftY, rightX, rightY;

	/* Cell coordinates to pixels: four per cell, from the window's top row.
	   In two steps because a Q8 position times four overflows 16 bits. */
	cx = (s16)((fp2int(player.pos.x) << 2) + ((player.pos.x & 0xff) >> 6));
	cy = (s16)(((fp2int(player.pos.y) - (s16)topRow) << 2) + ((player.pos.y & 0xff) >> 6));

	if(cy < -8 || cy > AUTOMAP_H + 8)
		return;

	tipX = (s16)(cx + ((f_cos * 5) >> 8));
	tipY = (s16)(cy + ((f_sin * 5) >> 8));
	baseX = (s16)(cx - ((f_cos * 3) >> 8));
	baseY = (s16)(cy - ((f_sin * 3) >> 8));

	/* Right of the facing is (-sin, cos). */
	leftX = (s16)(baseX + ((f_sin * 3) >> 8));
	leftY = (s16)(baseY - ((f_cos * 3) >> 8));
	rightX = (s16)(baseX - ((f_sin * 3) >> 8));
	rightY = (s16)(baseY + ((f_cos * 3) >> 8));

	bmDrawLine(tipX, tipY, leftX, leftY, blackBm);
	bmDrawLine(tipX, tipY, rightX, rightY, blackBm);
	bmDrawLine(leftX, leftY, rightX, rightY, blackBm);
}

void automapDraw(const u8 topRow)
{
	s16 x, y;
	u16 row;

	bmClearScreen();

	for(y = 0; y < AUTOMAP_ROWS; y++)
	{
		row = (u16)(topRow + y);

		if(row >= MAP_Y)
			break;

		for(x = 0; x < MAP_X; x++)
			drawCell((s16)(x << 2), (s16)(y << 2), map[row][x]);
	}

	drawPlayer(topRow);
}
