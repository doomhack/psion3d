#include <wlib.h>

#include "player.h"
#include "sprslot.h"
#include "fp_math.h"
#include "game_map.h"
#include "enemy.h"
#include "units.h"
#include "psion3d.h"

player_t player = {0};

const weapon_t weapons[] = 
{
	{24, 40, 225, 0, SPRITE_SLOT_PISTOL, 35, 96}, //Pistol
	{6, 10, 128, 0, SPRITE_SLOT_SMG, 35, 96}, //SMG
	{10, 20, 160, 1, SPRITE_SLOT_AR, 35, 96}, //AR
	{12, 25, 150, 2, SPRITE_SLOT_LMG, 35, 96}, //LMG
};


#define PLAYER_MOVE_SPEED_MPS flt2fp(4.0f)
#define PLAYER_TURN_SPEED_RADPS flt2fp(2.0f)

#define PLAYER_MOVE_TICK FP_METERS_PER_SECOND_TO_MAP_TICK(PLAYER_MOVE_SPEED_MPS)
#define PLAYER_TURN_TICK FP_RADIANS_PER_SECOND_TO_TICK(PLAYER_TURN_SPEED_RADPS)
#define PLAYER_MOVE_IMPULSE (PLAYER_MOVE_TICK >> 1)
#define PLAYER_TURN_IMPULSE (PLAYER_TURN_TICK >> 1)
#define PLAYER_MOMENTUM_DAMPING flt2fp(0.33f)
#define PLAYER_AIM_SPAN 30
#define PLAYER_MAX_SPREAD_SPANS 15

static f16 f_moveVel = 0;
static f16 f_turnVel = 0;
static u16 shotRand = 0x6d2b;

static u8 getShotSpan(const u8 accuracy)
{
	u16 spread = ((u16)(255 - accuracy) * (PLAYER_MAX_SPREAD_SPANS + 1)) >> 8;
	s16 span;

	if(spread == 0)
		return PLAYER_AIM_SPAN;

	shotRand = (u16)(shotRand * 25173 + 13849);
	span = PLAYER_AIM_SPAN + (s16)(shotRand % ((spread << 1) + 1)) - spread;

	if(span < 0)
		span = 0;
	else if(span > 59)
		span = 59;

	return (u8)span;
}

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
	f16 nx = player.pos.x + dx;
	f16 ny = player.pos.y;
	u16 cell = fmapCell(nx, ny);

	if(canWalk(cell) && !enemyBlocksPosition(nx, ny))
		player.pos.x = nx;

	nx = player.pos.x;
	ny = player.pos.y + dy;

	cell = fmapCell(nx, ny);

	if(canWalk(cell) && !enemyBlocksPosition(nx, ny))
		player.pos.y = ny;
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

static void updatePlayerWeapon(u8 keys)
{
	if(player.weaponState.shootCooldown > 0)
	{
		player.weaponState.shootCooldown--;

		if(player.weaponState.shootFrames > 0)
		{
			player.weaponState.shootFrames--;

			if(player.weaponState.shootFrames == 0)
				player.weaponState.weaponSpriteId = (player.currentWeapon->weaponSprite << 3) | 0;
		}
	}
	else
	{
		if(keys & KEY_FIRE)
		{
			player.weaponState.shootCooldown = player.currentWeapon->fireDelay;
			player.weaponState.weaponSpriteId = (player.currentWeapon->weaponSprite << 3) | 1;
			player.weaponState.shootFrames = 5;
			player.weaponState.shotSpan = getShotSpan(player.currentWeapon->accuracy);
			player.weaponState.shotPending = TRUE;
		}
	}
}

void initPlayer()
{
	player.pos.x = flt2fp(27.5f);
	player.pos.y = flt2fp(1.5f);

	player.pos.angle = 0;
	player.health = 100;
	f_moveVel = 0;
	f_turnVel = 0;

	player.currentWeapon = &weapons[0];
	player.weaponState.shootCooldown = 0;
	player.weaponState.shotPending = FALSE;
	player.weaponState.shotSpan = PLAYER_AIM_SPAN;
	player.weaponState.weaponSpriteId = (player.currentWeapon->weaponSprite << 3);
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

	player.pos.angle += f_turnVel;

	dx = fpmul(fpcos(player.pos.angle), f_moveVel);
	dy = fpmul(fpsin(player.pos.angle), f_moveVel);

	tryMove(dx, dy);

	updatePlayerWeapon(keys);
}
