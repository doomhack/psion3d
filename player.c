#include <wlib.h>

#include "player.h"
#include "fp_math.h"
#include "game_map.h"
#include "units.h"

position_t pos = {0};

#define PLAYER_MOVE_SPEED_MPS flt2fp(4.0f)
#define PLAYER_TURN_SPEED_RADPS flt2fp(2.0f)

#define PLAYER_MOVE_TICK FP_METERS_PER_SECOND_TO_MAP_TICK(PLAYER_MOVE_SPEED_MPS)
#define PLAYER_TURN_TICK FP_RADIANS_PER_SECOND_TO_TICK(PLAYER_TURN_SPEED_RADPS)
#define PLAYER_MOVE_IMPULSE (PLAYER_MOVE_TICK >> 1)
#define PLAYER_TURN_IMPULSE (PLAYER_TURN_TICK >> 1)
#define PLAYER_MOMENTUM_DAMPING flt2fp(0.5f)

static f16 f_moveVel = 0;
static f16 f_turnVel = 0;

static f16 clampFp(const f16 value, const f16 min, const f16 max)
{
	if(value < min)
		return min;

	if(value > max)
		return max;

	return value;
}

static void tryMove(const f16 dx, const f16 dy)
{
	f16 nx = pos.x + dx;
	f16 ny = pos.y;
	u16 cell = fmapCell(nx, ny);

	if(canWalk(cell))
		pos.x = nx;

	nx = pos.x;
	ny = pos.y + dy;

	cell = fmapCell(nx, ny);

	if(canWalk(cell))
		pos.y = ny;
}

static void addMoveImpulse(const f16 amount)
{
	f_moveVel = clampFp(f_moveVel + amount, -PLAYER_MOVE_TICK, PLAYER_MOVE_TICK);
}

static void addTurnImpulse(const f16 amount)
{
	f_turnVel = clampFp(f_turnVel + amount, -PLAYER_TURN_TICK, PLAYER_TURN_TICK);
}

void initPlayer()
{
	pos.x = flt2fp(27.5f);
	pos.y = flt2fp(1.5f);
	pos.angle = 0;
	f_moveVel = 0;
	f_turnVel = 0;
}

void updatePlayer()
{
	if(f_turnVel != 0)
	{
		pos.angle += f_turnVel;
		f_turnVel = fpmul(f_turnVel, PLAYER_MOMENTUM_DAMPING);
	}

	if(f_moveVel != 0)
	{
		const f16 dx = fpmul(fpcos(pos.angle), f_moveVel);
		const f16 dy = fpmul(fpsin(pos.angle), f_moveVel);

		tryMove(dx, dy);

		f_moveVel = fpmul(f_moveVel, PLAYER_MOMENTUM_DAMPING);
	}
}

void handlePlayerKey(const u16 key)
{
	if(key == W_KEY_LEFT)
	{
		addTurnImpulse(-PLAYER_TURN_IMPULSE);
	}
	else if(key == W_KEY_RIGHT)
	{
		addTurnImpulse(PLAYER_TURN_IMPULSE);
	}
	else if(key == W_KEY_UP)
	{
		addMoveImpulse(PLAYER_MOVE_IMPULSE);
	}
	else if(key == W_KEY_DOWN)
	{
		addMoveImpulse(-PLAYER_MOVE_IMPULSE);
	}
	else if(key == W_KEY_ESCAPE)
	{

	}
}
