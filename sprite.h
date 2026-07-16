#ifndef SPRITE_H
#define SPRITE_H

#include "fp_types.h"

typedef struct spritehit_t
{
	s16 spriteHeight;
	f16 f_spriteDist;
	u16 spanX;
	u8 spriteId;
	u8 mirrored;
	u8 enemyId;
} spritehit_t;

#define SPRITE_NO_ENEMY 255

HANDLE loadSprite(TEXT* baseName, u8 id);
u16 projectSprite(const f16 x, const f16 y, spritehit_t* hit, const f16 f_viewCos, const f16 f_viewSin);
void drawProjectedSprite(const spritehit_t* spriteHit);
void drawSprite(u8 spanX, u8 y, u8 spriteId);

#endif
