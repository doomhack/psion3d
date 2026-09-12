#include "fp_types.h"
#include "pickup.h"
#include "game_map.h"
#include "player.h"

/* Every weapon pickup is also that weapon's ammo, so the pool is topped up
   on each one. The weapon itself is granted and raised only on the first of
   its kind, so a second MP5 lying in a corridor refills without yanking a
   better gun out of the player's hands. */
static void giveWeapon(const u8 index)
{
	giveAmmo(weapons[index].ammoType);

	if(player.weaponsOwned & (1 << index))
		return;

	player.weaponsOwned |= (1 << index);

	selectWeapon(index);
}

/* The keycard is not checked at the door: picking it up opens every locked door
   on the level outright. A wall switch does the same thing, so the rewrite loop
   lives in game_map.c. */
static void giveKeycard(void)
{
	player.items |= PLAYER_ITEM_KEYCARD;

	unlockDoors();
}

void collectPickup(const u8 type)
{
	switch(type)
	{
		case PICKUP_TYPE_MP5:
			giveWeapon(WEAPON_SMG);
			break;

		case PICKUP_TYPE_AK47:
			giveWeapon(WEAPON_AR);
			break;

		case PICKUP_TYPE_M249:
			giveWeapon(WEAPON_LMG);
			break;

		case PICKUP_TYPE_KEYCARD:
			giveKeycard();
			break;
	}
}

void checkPickup(void)
{
	u16 x = (u16)fp2int(player.pos.x);
	u16 y = (u16)fp2int(player.pos.y);
	u16 cell = mapCell(x, y);

	/* Enemies share the sprite bit, and the player cannot stand in one. */
	if(!isSprite(cell) || isEnemy(cell))
		return;

	/* Clear the cell before the effect runs, so an effect that rewrites the
	   map cannot be undone by the removal. */
	updateCell(x, y, MAP_MASK_WALK);

	collectPickup(mapCellType(cell));
}

/* No wall bit: the ray cast only collects sprites from cells it can see
   through, and the walk bit is what lets the player step on it. Shared with
   enemy.c, which writes one of these over a corpse. */
u16 makePickupCell(const u8 type)
{
	return (MAP_MASK_SPRITE | MAP_MASK_WALK | SET_CELL_TYPE_ID(type));
}

/* Mirrors getEnemyCell. x and y are unused for now but kept in the signature
   so a pickup that needs to register world state has the same hook. */
u16 getPickupCell(u16 x, u16 y, s8 cell)
{
	u16 pickupType;

	switch(cell)
	{
		case 'I':
			pickupType = PICKUP_TYPE_AK47;
			break;

		case 'J':
			pickupType = PICKUP_TYPE_M249;
			break;

		case 'K':
			pickupType = PICKUP_TYPE_KEYCARD;
			break;

		case 'L':
			pickupType = 4;
			break;

		case 'M':
			pickupType = 5;
			break;

		case 'N':
			pickupType = 6;
			break;

		case 'O':
			pickupType = 7;
			break;

		default: //'H', the MP5.
			pickupType = PICKUP_TYPE_MP5;
			break;
	}

	return makePickupCell(pickupType);
}
