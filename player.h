#ifndef PLAYER_H
#define PLAYER_H

#include "fp_types.h"

typedef struct position_t
{
	f16 x, y;
	f16 angle;
} position_t;

extern position_t pos;

void initPlayer();
void updatePlayer();
void handlePlayerKey(const u16 key);

#endif
