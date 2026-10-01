/*  bmasm_pc.c - portable replacement for bmasm.a.

    The device clears the backbuffer with one `rep stosw` because the C loop
    that did it cost 4.1ms a frame, and the wall styles' column spans with an
    unrolled run of row instructions. Here the C is fine. */

#include <string.h>

#include "bitmap.h"

void bmClearScreen(void)
{
	memset(screenBm, 0, sizeof(screenBm));
}

/*  The column spans bmasm.a runs unrolled, as they were in bitmap.c. */
#define BM_WIDTH 256
#define BM_HEIGHT 160
#define BM_ROW_BYTES 32

void bmFillRect4(s16 x, s16 y, s16 h, u8* bm)
{
	u8* row;
	u8 mask;

	if(h <= 0 || x < 0 || x > BM_WIDTH - 4)
		return;

	if(y < 0)
	{
		h += y;
		y = 0;
	}

	if(y + h > BM_HEIGHT)
		h = BM_HEIGHT - y;

	if(h <= 0)
		return;

	row = bm + (y << 5) + (x >> 3);
	mask = (x & 4) ? 0xf0 : 0x0f;

	do
	{
		*row |= mask;
		row += BM_ROW_BYTES;
	} while(--h);
}


void bmClearRect4(s16 x, s16 y, s16 h, u8* bm)
{
	u8* row;
	u8 mask;

	if(h <= 0 || x < 0 || x > BM_WIDTH - 4)
		return;

	if(y < 0)
	{
		h += y;
		y = 0;
	}

	if(y + h > BM_HEIGHT)
		h = BM_HEIGHT - y;

	if(h <= 0)
		return;

	row = bm + (y << 5) + (x >> 3);
	mask = (x & 4) ? 0x0f : 0xf0;

	do
	{
		*row &= mask;
		row += BM_ROW_BYTES;
	} while(--h);
}

void bmFillPattern4(s16 x, s16 y, s16 h, u8* bm)
{
	u8* row;
	u8 mask;
	u8 keepMask;
	u8 pat;

	if(h <= 0 || x < 0 || x > BM_WIDTH - 4)
		return;

	if(y < 0)
	{
		h += y;
		y = 0;
	}

	if(y + h > BM_HEIGHT)
		h = BM_HEIGHT - y;

	if(h <= 0)
		return;

	row = bm + (y << 5) + (x >> 3);
	mask = (x & 4) ? 0xf0 : 0x0f;
	keepMask = ~mask;
	pat = (y & 1) ? (0x55 & mask) : (0xaa & mask);

	do
	{
		*row = (*row & keepMask) | pat;
		pat ^= mask;
		row += BM_ROW_BYTES;
	} while(--h);
}

