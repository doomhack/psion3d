#ifndef PLAYER_H
#define PLAYER_H

#include "fp_types.h"

#define WEAPON_PISTOL 0
#define WEAPON_SMG 1
#define WEAPON_AR 2
#define WEAPON_LMG 3

#define SPRITE_SLOT_PISTOL 5
#define SPRITE_SLOT_SMG 6
#define SPRITE_SLOT_AR 7
#define SPRITE_SLOT_LMG 8

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
} weapon_state_t;

typedef struct position_t
{
	f16 x, y;
	f16 angle;
} position_t;

typedef struct player_t
{
	position_t pos;
	u8 health;
	const weapon_t* currentWeapon;
	weapon_state_t weaponState;
	u8 weaponsOwned; //bitmask of weapons owned.

} player_t;


extern player_t player;

void initPlayer(void);
void updatePlayer(u8 keys);

#endif
