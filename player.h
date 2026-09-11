#ifndef PLAYER_H
#define PLAYER_H

#include "fp_types.h"

#define WEAPON_PISTOL 0
#define WEAPON_SMG 1
#define WEAPON_AR 2
#define WEAPON_LMG 3

/* Inventory bits held in player_t.items. */
#define PLAYER_ITEM_KEYCARD 0x01

/* Weapon switch animation phases. The old weapon lowers out of view, the
   sprite is swapped while it is off screen, then the new one is raised. */
#define WEAPON_SWITCH_NONE 0
#define WEAPON_SWITCH_LOWERING 1
#define WEAPON_SWITCH_RAISING 2

typedef struct weapon_t
{
	u8 fireDelay; //Ticks between sucessive fire rounds.
	u8 damage; //Damge dealt.
	u8 accuracy; //0..255 chance of hitting.
	u8 ammoType; //Index of ammo type.
	u8 weaponSprite; //Sprite Slot.
	u8 spanX; //X Spand to draw sprite.
	u8 y; //Y pos to draw sprite.
} weapon_t;

extern const weapon_t weapons[];

typedef struct weapon_state_t
{
	u8 weaponSpriteId; //Sprite ID to draw.
	u8 shootCooldown; //Cooldown between bullets.
	u8 shootFrames; //Show firing frame time.
	u8 shotPending; //A fired round waiting for the current frame's visibility data.
	u8 shotSpan; //Horizontal ray span selected by weapon accuracy.
	u8 switchPhase; //WEAPON_SWITCH_* state of the raise/lower animation.
	u8 switchOffset; //Rows the weapon sprite is currently lowered by.
	u8 pendingWeapon; //Weapon index to raise once the old one is out of view.
	u8 recoilOffset; //Rows the weapon sprite is kicked down by after firing.
} weapon_state_t;

typedef struct position_t
{
	f16 x, y;
	f16 angle;
} position_t;

/* Which screen edge the hurt flash paints: the one nearest the shooter. */
#define PLAYER_HIT_LEFT 1
#define PLAYER_HIT_RIGHT 2
#define PLAYER_HIT_FRONT 4
#define PLAYER_HIT_BACK 8

typedef struct player_t
{
	position_t pos;
	u8 health;
	const weapon_t* currentWeapon;
	weapon_state_t weaponState;
	u8 weaponsOwned; //bitmask of weapons owned.
	u8 items; //bitmask of PLAYER_ITEM_* held.
	u8 hitFlash; //Frames left of the hurt border.
	u8 hitDir; //PLAYER_HIT_* edge to paint it on.

} player_t;


extern player_t player;

void initPlayer(void);
void updatePlayer(u16 keys);

/* Apply a hit from a shooter at fromX, fromY: damage, a shove away from
   them, a jolt to the view, and a flash on the edge of the screen they are on. */
void hurtPlayer(const u8 damage, const f16 fromX, const f16 fromY);
void selectWeapon(const u8 index);

#endif
