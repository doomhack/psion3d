#ifndef AUTOMAP_MODULE_H
#define AUTOMAP_MODULE_H

#include "fp_types.h"

/*  The level plan for the pause menu's Map screen. Rendered into screenBm
    with the bitmap.c primitives - 4x4 pixels per cell, so the 64 columns are
    exactly the buffer's 256 - and put on screen with uiBlitMap. */

#define AUTOMAP_ROWS 26
#define AUTOMAP_H (AUTOMAP_ROWS * 4)

/*  Draw map rows topRow .. topRow + AUTOMAP_ROWS into rows 0 .. AUTOMAP_H
    of both planes, and the player on top if they are in the window. */
void automapDraw(const u8 topRow);

#endif
