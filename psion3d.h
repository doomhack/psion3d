#ifndef PSION3D_H
#define PSION3D_H

/*  No SDK header: walls.c, labwall.c, enemy.c, draw.c and player.c all include
    this, and none of them touch PLIB or WLIB. The window rectangles that used
    to live here needed P_RECT and were only ever read by psion3d.c, so they
    moved there. */

#include "fp_types.h"
#include "fp_math.h"
#include "game_map.h"
#include "player.h"

static const u16 BM_BLK = 0;
static const u16 BM_GRY = 1;


#define KEY_UP          (1)
#define KEY_DOWN        (1 << 1)
#define KEY_LEFT        (1 << 2)
#define KEY_RIGHT       (1 << 3)
#define KEY_FIRE        (1 << 4)
#define KEY_WEAPON_1    (1 << 5)
#define KEY_WEAPON_2    (1 << 6)
#define KEY_WEAPON_3    (1 << 7)
#define KEY_WEAPON_4    (1 << 8)
#define KEY_10          (1 << 9)
#define KEY_11          (1 << 10)
#define KEY_12          (1 << 11)
#define KEY_13          (1 << 12)
#define KEY_14          (1 << 13)
#define KEY_15          (1 << 14)
#define KEY_16          (1 << 15)

extern f16 dbgval;

#endif
