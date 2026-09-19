#ifndef UI_PSION_H
#define UI_PSION_H

#include "fp_types.h"

/*  The menu window on the device. Only psion3d.c calls these; the PC host
    has its own equivalents around menu_pc.cpp. */

/*  Window handle passed to wCreateWindow, so WM_REDRAW events name it. */
#define MENU_WIN 3

void createMenuWindow(void);
void menuWindowShow(u16 on);
void menuWindowInvalidate(void);
void menuWindowRedraw(void);

#endif
