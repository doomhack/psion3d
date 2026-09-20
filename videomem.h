#ifndef VIDEOMEM_H
#define VIDEOMEM_H

#include "fp_types.h"

/*  Copy both planes of the backbuffer straight into video RAM (segment 0x40,
    60 bytes a row, the black plane at 0 and the grey plane at 9600), at
    x = 120: 15 words a row, so the 16 spare columns each 32 byte row holds
    beyond the 240 the LCD shows stay in the buffer and never land on the HUD
    panel to the right of the view. The buffer keeps 32 byte rows because
    that makes every row address a shift; the spare columns are a spill area
    for drawing that runs past the right edge, and the PC host's --gutter
    shows what lands there. */
void blitVideoMem(u8* black, u8* grey);

#endif
