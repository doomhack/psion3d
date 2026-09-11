#ifndef DECOR_H
#define DECOR_H

#include "fp_types.h"

/* Decoration kind, stored in the map cell's type nibble. A sprite slot holds
   eight frames, so pickups take types 0..7 and decorations 8..15: the low
   three bits are the frame within SPRITE_SLOT_DECORATIONS, and DECOR_TYPE_BIT
   is what tells draw.c which slot a sprite cell draws from. */
#define DECOR_TYPE_BIT 8

#define DECOR_TYPE_CAMERA 0
#define DECOR_TYPE_COMPUTER 1

/* Types 2 through 7 are reserved for map characters '3'..'8'. */

u16 makeDecorCell(const u8 type);
u16 getDecorCell(u16 x, u16 y, s8 cell);

#endif
