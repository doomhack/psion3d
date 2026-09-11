#include "fp_types.h"
#include "decor.h"
#include "game_map.h"

/* Set dressing: a sprite the player and enemies walk around, never onto. No
   wall bit, so the ray runs on past it and collects it like any sprite; no
   walk bit, so updatePlayer and enemyTryMoveTo both refuse the cell. */
u16 makeDecorCell(const u8 type)
{
	return (MAP_MASK_SPRITE | SET_CELL_TYPE_ID(DECOR_TYPE_BIT | type));
}

/* Mirrors getPickupCell. Digits '1'..'8' are decorations 0..7, so the map
   character is one more than the frame it draws. */
u16 getDecorCell(u16 x, u16 y, s8 cell)
{
	return makeDecorCell((u8)(cell - '1'));
}
