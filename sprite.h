#ifndef SPRITE_H
#define SPRITE_H

#include "fp_types.h"

typedef struct spritehit_t
{
	s16 spriteHeight;
	f16 f_spriteDist;
	u16 spanX;
} spritehit_t;

u16 projectSprite(const u16 x, const u16 y, spritehit_t* hit, const f16 f_viewCos, const f16 f_viewSin);
void drawSprite(const spritehit_t* spriteHit);

#endif
