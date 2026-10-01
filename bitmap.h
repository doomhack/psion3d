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

/*  Both planes to zero: one `rep stosw` over all BM_SCREEN_WORDS (5120,
    hard-coded in bmasm.a), in assembler because the C loop, unrolled
    sixteen times, measured 4.1ms a frame. Touches AX only; pc/src/bmasm_pc.c
    is the C twin. */
#pragma save, call(reg_saved=>(bx,cx,dx,si,di,es,ds,st1,st2))
void bmClearScreen(void);
#pragma restore
void bmFillRect(s16 x, s16 y, s16 w, s16 h, u8* bm);
/*  The 4 pixel column spans the wall styles are built from, in bmasm.a: the
    C clipping in registers, then a jump into an unrolled run of 160 rows.
    A fill or clear row is one `or` (or `and`) in place of a five
    instruction loop that cost 2us a row in C; a dither row is a load,
    `and`, `or` and store in place of twelve, with the two alternating
    patterns fixed to the even and odd rows (TASKS.md task 26). The same
    convention the C versions compiled to: arguments in AX, BX, CX, DX,
    which they clobber. pc/src/bmasm_pc.c holds the C they replace. */
#pragma save, call(reg_param=>(ax,bx,cx,dx), reg_saved=>(si,di,es,ds,st1,st2))
void bmFillRect4(s16 x, s16 y, s16 h, u8* bm);
void bmClearRect4(s16 x, s16 y, s16 h, u8* bm);
void bmFillPattern4(s16 x, s16 y, s16 h, u8* bm);
#pragma restore
void bmFillCol1(s16 x, s16 y, s16 h, u8* bm);
void bmDrawRect(s16 x, s16 y, s16 w, s16 h, u8* bm);
void bmClearRect(s16 x, s16 y, s16 w, s16 h, u8* bm);
void bmFillPattern(s16 x, s16 y, s16 w, s16 h, u8* bm);
void bmDrawLine(s16 start_x, s16 start_y, s16 end_x, s16 end_y, u8* bm);
void bmXorLine(s16 start_x, s16 start_y, s16 end_x, s16 end_y, u8* bm);

#endif
