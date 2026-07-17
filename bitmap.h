#ifndef BITMAP_H
#define BITMAP_H

#include "fp_types.h"

#define BM_WIDTH 256
#define BM_HEIGHT 160
#define BM_ROW_BYTES 64
#define BM_SCREEN_BYTES (BM_ROW_BYTES * BM_HEIGHT)
#define BM_SCREEN_WORDS (BM_SCREEN_BYTES / 2)

#define BM_PLANE_BLACK 0
#define BM_PLANE_GREY 1

#define BM_OP_NO_OP 0
#define BM_OP_FILL 1
#define BM_OP_CLEAR 2
#define BM_OP_PATTERN 3

extern u16 screenBm[BM_SCREEN_WORDS];

void bmClearScreen(void);
void bmDraw4(s16 x, s16 y, s16 h, u16 blackOp, u16 greyOp);
void bmFillRect(s16 x, s16 y, s16 w, s16 h, u16 plane);
void bmDrawRect(s16 x, s16 y, s16 w, s16 h, u16 plane);
void bmClearRect(s16 x, s16 y, s16 w, s16 h, u16 plane);
void bmFillPattern(s16 x, s16 y, s16 w, s16 h, u16 plane);
void bmDrawLine(s16 start_x, s16 start_y, s16 end_x, s16 end_y, u16 plane);

#endif
