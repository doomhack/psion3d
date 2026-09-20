#ifndef UI_PSION_H
#define UI_PSION_H

#include "fp_types.h"

/*  The menu and HUD windows on the device. Only psion3d.c calls these; the
    PC host has its own equivalents around menu_pc.cpp. */

/*  Window handles passed to wCreateWindow, so WM_REDRAW events name them. */
#define HUD_WIN 1
#define MENU_WIN 3

/*  Create the HUD window first: it is the bottom of the stack, and the game
    window sits over its middle 240 columns. */
void createHudWindow(void);
void hudWindowInvalidate(void);
void hudWindowRedraw(void);

/*  The menu window is created last, so it stacks over everything. */
void createMenuWindow(void);
void menuWindowShow(u16 on);
void menuWindowInvalidate(void);
void menuWindowRedraw(void);

#endif
