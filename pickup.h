#ifndef PICKUP_H
#define PICKUP_H

#include "fp_types.h"

/* Pickup kind, stored in the map cell's type nibble. The value doubles as the
   frame index into SPRITE_SLOT_PICKUPS, so pup0..pup3 line up with these. */
#define PICKUP_TYPE_MP5 0
#define PICKUP_TYPE_AK47 1
#define PICKUP_TYPE_M249 2
#define PICKUP_TYPE_KEYCARD 3

/* Types 4 through 7 are reserved for map characters 'L'..'O'. Types 8 and up
   are decorations, see DECOR_TYPE_BIT in decor.h. */

u16 makePickupCell(const u8 type);
u16 getPickupCell(u16 x, u16 y, s8 cell);
void checkPickup(void);
void collectPickup(const u8 type);

#endif
