/*  menu_pc.h - what the Qt layer needs from the PC menu renderer. */

#ifndef MENU_PC_H
#define MENU_PC_H

#include <QImage>

/*  The menu window as drawn so far, composed from its two planes into the
    LCD palette. Valid after any menu draw; 480x160. */
const QImage &uiPcImage();

#endif
