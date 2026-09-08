#include "player.h"
#include "sprslot.h"
#include "fp_math.h"
#include "game_map.h"
#include "enemy.h"
#include "units.h"
#include "psion3d.h"
#include "pickup.h"

player_t player = {0};

const weapon_t weapons[] = 
{
	{24, 40, 240, 0, SPRITE_SLOT_PISTOL, 35, 96}, //Pistol
	{6, 10, 160, 0, SPRITE_SLOT_SMG, 35, 96}, //SMG
	{10, 20, 224, 1, SPRITE_SLOT_AR, 35, 96}, //AR
	{12, 25, 192, 2, SPRITE_SLOT_LMG, 35, 96}, //LMG
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

/* Sprites are 64 rows tall and every weapon sits at y 96, so lowering by a
   full sprite height puts the top edge on the bottom of the screen and
   drawSprite clips the whole thing away. */
#define WEAPON_SWITCH_TRAVEL 64
#define WEAPON_SWITCH_STEP 8 //Rows per tick, so eight ticks each way.

#define WEAPON_RECOIL_KICK 8 //Rows the sprite drops on firing.
#define WEAPON_RECOIL_STEP 2 //Rows recovered per tick, so four ticks to settle.

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

/* Begin lowering the current weapon so that index can be raised in its place.
   Retargeting part way through simply changes what comes back up. */
void selectWeapon(const u8 index)
{
	//Weapon 1 is always owned, the rest need a pickup.
	if(index != WEAPON_PISTOL && !(player.weaponsOwned & (1 << index)))
		return;

	if(player.weaponState.switchPhase == WEAPON_SWITCH_NONE)
	{
		//Keys are level triggered, so holding one must not restart the slide.
		if(player.currentWeapon == &weapons[index])
			return;
	}
	else if(player.weaponState.pendingWeapon == index)
	{
		return;
	}

	player.weaponState.pendingWeapon = index;
	player.weaponState.switchPhase = WEAPON_SWITCH_LOWERING;
}

static void updateWeaponSwitch(void)
{
	if(player.weaponState.switchPhase == WEAPON_SWITCH_LOWERING)
	{
		player.weaponState.switchOffset += WEAPON_SWITCH_STEP;

		if(player.weaponState.switchOffset >= WEAPON_SWITCH_TRAVEL)
		{
			player.weaponState.switchOffset = WEAPON_SWITCH_TRAVEL;

			/* Out of sight, so this is the moment to swap the sprite. Doing it
			   here also keeps a shot fired this tick on the old weapon. */
			player.currentWeapon = &weapons[player.weaponState.pendingWeapon];
			player.weaponState.weaponSpriteId = (u8)(player.currentWeapon->weaponSprite << 3);
			player.weaponState.shootFrames = 0;
			player.weaponState.recoilOffset = 0; //The new weapon comes up settled.
			player.weaponState.switchPhase = WEAPON_SWITCH_RAISING;
		}
	}
	else if(player.weaponState.switchPhase == WEAPON_SWITCH_RAISING)
	{
		if(player.weaponState.switchOffset > WEAPON_SWITCH_STEP)
		{
			player.weaponState.switchOffset -= WEAPON_SWITCH_STEP;
		}
		else
		{
			player.weaponState.switchOffset = 0;
			player.weaponState.switchPhase = WEAPON_SWITCH_NONE;
		}
	}
}

static void updateWeaponRecoil(void)
{
	if(player.weaponState.recoilOffset == 0)
		return;

	if(player.weaponState.recoilOffset > WEAPON_RECOIL_STEP)
		player.weaponState.recoilOffset -= WEAPON_RECOIL_STEP;
	else
		player.weaponState.recoilOffset = 0;
}

static void updatePlayerWeapon(u16 keys)
{
	/* Recovery runs before the trigger so that a shot fired this tick still
	   renders at the full kick. Several ticks can pass between frames. */
	updateWeaponRecoil();

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
	else if(player.weaponState.switchPhase == WEAPON_SWITCH_NONE)
	{
		//No firing while the weapon is off screen being swapped.
		if(keys & KEY_FIRE)
		{
			player.weaponState.shootCooldown = player.currentWeapon->fireDelay;
			player.weaponState.weaponSpriteId = (player.currentWeapon->weaponSprite << 3) | 1;
			player.weaponState.shootFrames = 5;
			player.weaponState.shotSpan = getShotSpan(player.currentWeapon->accuracy);
			player.weaponState.shotPending = TRUE;
			player.weaponState.recoilOffset = WEAPON_RECOIL_KICK;

			//Gunfire gives the player away, whether or not the round hits.
			alertEnemies((u8)fp2int(player.pos.x), (u8)fp2int(player.pos.y));
		}
	}

	if(keys & KEY_WEAPON_1)
		selectWeapon(WEAPON_PISTOL);
	else if(keys & KEY_WEAPON_2)
		selectWeapon(WEAPON_SMG);
	else if(keys & KEY_WEAPON_3)
		selectWeapon(WEAPON_AR);
	else if(keys & KEY_WEAPON_4)
		selectWeapon(WEAPON_LMG);

	updateWeaponSwitch();
}

void initPlayer()
{
	player.pos.x = flt2fp(27.5f);
	player.pos.y = flt2fp(1.5f);

	player.pos.angle = 0;
	player.health = 100;
	f_moveVel = 0;
	f_turnVel = 0;

	player.weaponsOwned = 0; //Only the pistol is free; the rest are pickups.
	player.items = 0;
	player.currentWeapon = &weapons[WEAPON_PISTOL];
	player.weaponState.shootCooldown = 0;
	player.weaponState.shootFrames = 0;
	player.weaponState.shotPending = FALSE;
	player.weaponState.shotSpan = PLAYER_AIM_SPAN;
	player.weaponState.weaponSpriteId = (player.currentWeapon->weaponSprite << 3);
	player.weaponState.switchPhase = WEAPON_SWITCH_NONE;
	player.weaponState.switchOffset = 0;
	player.weaponState.pendingWeapon = WEAPON_PISTOL;
	player.weaponState.recoilOffset = 0;
}

void updatePlayer(u16 keys)
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

	/* With no velocity dx and dy are both zero and tryMove cannot change the
	   position, so skip it along with its two enemy list scans. */
	if(f_moveVel != 0)
	{
		dx = fpmul(fpcos(player.pos.angle), f_moveVel);
		dy = fpmul(fpsin(player.pos.angle), f_moveVel);

		tryMove(dx, dy);

		/* Standing on a pickup needs a move to reach it, so this rides the
		   same guard as tryMove rather than costing an idle tick. */
		checkPickup();
	}

	updatePlayerWeapon(keys);
}
