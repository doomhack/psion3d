#include "gameloop.h"

#include "bitmap.h"
#include "draw.h"
#include "enemy.h"
#include "player.h"

u16 keys = 0;

s16 tickDelta(const u16 later, const u16 earlier)
{
	return (s16)(later - earlier);
}

u16 tickElapsed(const u16 later, const u16 earlier)
{
	return (u16)(later - earlier);
}

u16 gameRunTicks(u16 gameTime, const u16 realTime)
{
	while(tickDelta(realTime, gameTime) > 0)
	{
		updatePlayer(keys);
		runAI();

		gameTime++;
	}

	bmClearScreen();
	draw();

	return gameTime;
}
