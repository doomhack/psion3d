#include <wlib.h>

#include "player.h"
#include "fp_math.h"
#include "game_map.h"
#include "units.h"
#include "psion3d.h"

position_t pos = {0};

#define PLAYER_MOVE_SPEED_MPS flt2fp(4.0f)
#define PLAYER_TURN_SPEED_RADPS flt2fp(2.0f)

#define PLAYER_MOVE_TICK FP_METERS_PER_SECOND_TO_MAP_TICK(PLAYER_MOVE_SPEED_MPS)
#define PLAYER_TURN_TICK FP_RADIANS_PER_SECOND_TO_TICK(PLAYER_TURN_SPEED_RADPS)
#define PLAYER_MOVE_IMPULSE (PLAYER_MOVE_TICK >> 1)
#define PLAYER_TURN_IMPULSE (PLAYER_TURN_TICK >> 1)
#define PLAYER_MOMENTUM_DAMPING flt2fp(0.33f)

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

static f16 dampMomentum(const f16 value)
{
	f16 damped;

	if(value == 0)
		return 0;

	damped = fpmul(value, PLAYER_MOMENTUM_DAMPING);

	if(damped == value)
		return 0;

	return damped;
}

void initPlayer()
{
	pos.x = flt2fp(27.5f);
	pos.y = flt2fp(1.5f);

	pos.angle = 0;
	f_moveVel = 0;
	f_turnVel = 0;
}

void updatePlayer(u8 keys)
{
	f16 dx, dy;

	if(keys & (KEY_LEFT | KEY_RIGHT))
	{
		if(keys & KEY_LEFT)
			addTurnImpulse(-PLAYER_TURN_IMPULSE);

		if(keys & KEY_RIGHT)
			addTurnImpulse(PLAYER_TURN_IMPULSE);
	}
	else
	{
		f_turnVel = dampMomentum(f_turnVel);
	}

	if(keys & (KEY_UP | KEY_DOWN))
	{
		if(keys & KEY_UP)
			addMoveImpulse(PLAYER_MOVE_IMPULSE);

		if(keys & KEY_DOWN)
			addMoveImpulse(-PLAYER_MOVE_IMPULSE);
	}
	else
	{
		f_moveVel = dampMomentum(f_moveVel);
	}

	pos.angle += f_turnVel;

	dx = fpmul(fpcos(pos.angle), f_moveVel);
	dy = fpmul(fpsin(pos.angle), f_moveVel);

	tryMove(dx, dy);
}
