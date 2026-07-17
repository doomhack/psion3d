#include "bitmap.h"

u16 screenBm[BM_SCREEN_WORDS];

static u16 clipRect(s16* x, s16* y, s16* w, s16* h)
{
	s16 right;
	s16 bottom;

	if(*w <= 0 || *h <= 0)
		return FALSE;

	right = *x + *w;
	bottom = *y + *h;

	if(*x < 0)
		*x = 0;

	if(*y < 0)
		*y = 0;

	if(right > BM_WIDTH)
		right = BM_WIDTH;

	if(bottom > BM_HEIGHT)
		bottom = BM_HEIGHT;

	*w = right - *x;
	*h = bottom - *y;

	return *w > 0 && *h > 0;
}

static u8 planeMask(u16 plane, u8 pixels)
{
	return plane == BM_PLANE_GREY ? pixels << 4 : pixels;
}

static void writePixel(s16 x, s16 y, u16 plane, u16 value)
{
	u8* bm = (u8*)screenBm;
	u8 mask = planeMask(plane, 1 << (x & 3));
	u16 offset = (y << 6) + (x >> 2);

	if(value)
		bm[offset] |= mask;
	else
		bm[offset] &= ~mask;
}

static void fillSpan(s16 x, s16 y, s16 w, u16 plane, u16 value)
{
	s16 end = x + w;

	while(x < end)
	{
		writePixel(x, y, plane, value);
		x++;
	}
}

static void patternSpan(s16 x, s16 y, s16 w, u16 plane)
{
	s16 end = x + w;

	while(x < end)
	{
		writePixel(x, y, plane, (x ^ y) & 1);
		x++;
	}
}

void bmClearScreen()
{
	u16 i = BM_SCREEN_WORDS >> 4;
	u16* bm = screenBm;

	do
	{
		*bm++ = 0;
		*bm++ = 0;
		*bm++ = 0;
		*bm++ = 0;
		*bm++ = 0;
		*bm++ = 0;
		*bm++ = 0;
		*bm++ = 0;
		*bm++ = 0;
		*bm++ = 0;
		*bm++ = 0;
		*bm++ = 0;
		*bm++ = 0;
		*bm++ = 0;
		*bm++ = 0;
		*bm++ = 0;
	} while(--i);
}

void bmDraw4(s16 x, s16 y, s16 h, u16 blackOp, u16 greyOp)
{
	u8* row;
	u8 keepMask = 0;
	u8 value = 0;
	u8 pattern = 0;
	u8 patternFlip = 0;

	if(h <= 0 || x < 0 || x > BM_WIDTH - 4 || (x & 3))
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

	row = (u8*)screenBm + (y << 6) + (x >> 2);

	if(blackOp == BM_OP_NO_OP)
		keepMask |= 0x0f;
	else if(blackOp == BM_OP_FILL)
		value |= 0x0f;
	else if(blackOp == BM_OP_PATTERN)
	{
		pattern |= (y & 1) ? 0x05 : 0x0a;
		patternFlip |= 0x0f;
	}

	if(greyOp == BM_OP_NO_OP)
		keepMask |= 0xf0;
	else if(greyOp == BM_OP_FILL)
		value |= 0xf0;
	else if(greyOp == BM_OP_PATTERN)
	{
		pattern |= (y & 1) ? 0x50 : 0xa0;
		patternFlip |= 0xf0;
	}

	value |= pattern;

	if(keepMask)
	{
		do
		{
			*row = (*row & keepMask) | value;
			value ^= patternFlip;
			row += BM_ROW_BYTES;
		} while(--h);
	}
	else
	{
		do
		{
			*row = value;
			value ^= patternFlip;
			row += BM_ROW_BYTES;
		} while(--h);
	}
}

void bmFillRect(s16 x, s16 y, s16 w, s16 h, u16 plane)
{
	s16 yy;

	if(!clipRect(&x, &y, &w, &h))
		return;

	for(yy = y; yy < y + h; yy++)
		fillSpan(x, yy, w, plane, TRUE);
}

void bmDrawRect(s16 x, s16 y, s16 w, s16 h, u16 plane)
{
	s16 yy;
	s16 right;
	s16 bottom;

	if(!clipRect(&x, &y, &w, &h))
		return;

	right = x + w - 1;
	bottom = y + h - 1;

	fillSpan(x, y, w, plane, TRUE);

	if(bottom != y)
		fillSpan(x, bottom, w, plane, TRUE);

	if(h <= 2)
		return;

	for(yy = y + 1; yy < bottom; yy++)
	{
		writePixel(x, yy, plane, TRUE);

		if(right != x)
			writePixel(right, yy, plane, TRUE);
	}
}

void bmClearRect(s16 x, s16 y, s16 w, s16 h, u16 plane)
{
	s16 yy;

	if(!clipRect(&x, &y, &w, &h))
		return;

	for(yy = y; yy < y + h; yy++)
		fillSpan(x, yy, w, plane, FALSE);
}

void bmFillPattern(s16 x, s16 y, s16 w, s16 h, u16 plane)
{
	s16 yy;

	if(!clipRect(&x, &y, &w, &h))
		return;

	for(yy = y; yy < y + h; yy++)
		patternSpan(x, yy, w, plane);
}

void bmDrawLine(s16 start_x, s16 start_y, s16 end_x, s16 end_y, u16 plane)
{
	s16 dx = end_x > start_x ? end_x - start_x : start_x - end_x;
	s16 dy = end_y > start_y ? end_y - start_y : start_y - end_y;
	s16 sx = start_x < end_x ? 1 : -1;
	s16 sy = start_y < end_y ? 1 : -1;
	s16 err = dx - dy;
	s16 e2;

	while(TRUE)
	{
		if(start_x >= 0 && start_x < BM_WIDTH && start_y >= 0 && start_y < BM_HEIGHT)
			writePixel(start_x, start_y, plane, TRUE);

		if(start_x == end_x && start_y == end_y)
			break;

		e2 = err + err;

		if(e2 > -dy)
		{
			err -= dy;
			start_x += sx;
		}

		if(e2 < dx)
		{
			err += dx;
			start_y += sy;
		}
	}
}
