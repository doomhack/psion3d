#ifndef BITMAP_H
#define BITMAP_H

#include "fp_types.h"

#define BM_WORDS 2560
#define BM_BYTES (BM_WORDS * 2)
#define BM_SCREEN_WORDS (BM_WORDS * 2)
#define BM_SCREEN_BYTES (BM_SCREEN_WORDS * 2)

extern u16 screenBm[BM_SCREEN_WORDS];
extern u8* blackBm;
extern u8* greyBm;

void bmClearScreen(void);
void bmFillRect(s16 x, s16 y, s16 w, s16 h, u8* bm);
void bmFillRect4(s16 x, s16 y, s16 h, u8* bm);
void bmDrawRect(s16 x, s16 y, s16 w, s16 h, u8* bm);
void bmClearRect(s16 x, s16 y, s16 w, s16 h, u8* bm);
void bmClearRect4(s16 x, s16 y, s16 h, u8* bm);
void bmFillPattern(s16 x, s16 y, s16 w, s16 h, u8* bm);
void bmFillPattern4(s16 x, s16 y, s16 h, u8* bm);
void bmDrawLine(s16 start_x, s16 start_y, s16 end_x, s16 end_y, u8* bm);

#endif
