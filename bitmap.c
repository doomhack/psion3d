#include "bitmap.h"

u16 screenBm[BM_SCREEN_WORDS];
//u16* blackBm = &screenBm[0];
//u16* greyBm = &screenBm[BM_WORDS];

u8* blackBm = (u8*)&screenBm[0];
u8* greyBm = (u8*)&screenBm[BM_WORDS];

#define BM_WIDTH 256
#define BM_HEIGHT 160
#define BM_ROW_BYTES 32

static u8 leftMask[8] =
{
	0xff, 0xfe, 0xfc, 0xf8, 0xf0, 0xe0, 0xc0, 0x80
};

static u8 rightMask[8] =
{
	0x01, 0x03, 0x07, 0x0f, 0x1f, 0x3f, 0x7f, 0xff
};

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

static void setPixel(s16 x, s16 y, u8* bm)
{
	bm[(y << 5) + (x >> 3)] |= (1 << (x & 7));
}

static void fillSpan(s16 x, s16 y, s16 w, u8* bm, u8 value)
{
	u8* row = bm + (y << 5);
	u16 startByte = x >> 3;
	u16 endByte = (x + w - 1) >> 3;
	u8 startBit = x & 7;
	u8 endBit = (x + w - 1) & 7;
	u8 mask;
	u16 i;

	if(startByte == endByte)
	{
		mask = leftMask[startBit] & rightMask[endBit];

		if(value)
			row[startByte] |= mask;
		else
			row[startByte] &= ~mask;

		return;
	}

	mask = leftMask[startBit];

	if(value)
		row[startByte] |= mask;
	else
		row[startByte] &= ~mask;

	for(i = startByte + 1; i < endByte; i++)
		row[i] = value;

	mask = rightMask[endBit];

	if(value)
		row[endByte] |= mask;
	else
		row[endByte] &= ~mask;
}

static void patternSpan(s16 x, s16 y, s16 w, u8* bm)
{
	u8* row = bm + (y << 5);
	u16 startByte = x >> 3;
	u16 endByte = (x + w - 1) >> 3;
	u8 startBit = x & 7;
	u8 endBit = (x + w - 1) & 7;
	u8 pat = (y & 1) ? 0x55 : 0xaa;
	u8 mask;
	u16 i;

	if(startByte == endByte)
	{
		mask = leftMask[startBit] & rightMask[endBit];
		row[startByte] = (row[startByte] & ~mask) | (pat & mask);
		return;
	}

	mask = leftMask[startBit];
	row[startByte] = (row[startByte] & ~mask) | (pat & mask);

	for(i = startByte + 1; i < endByte; i++)
		row[i] = pat;

	mask = rightMask[endBit];
	row[endByte] = (row[endByte] & ~mask) | (pat & mask);
}

void bmClearScreen()
{
	u16 i = 0;
	u16* blackBMW = (u16*)blackBm;
	u16* greyBMW = (u16*)greyBm;

	do
	{
		blackBMW[i] = 0;
		greyBMW[i] = 0;
		i++;

		blackBMW[i] = 0;
		greyBMW[i] = 0;
		i++;

		blackBMW[i] = 0;
		greyBMW[i] = 0;
		i++;

		blackBMW[i] = 0;
		greyBMW[i] = 0;
		i++;

		blackBMW[i] = 0;
		greyBMW[i] = 0;
		i++;

		blackBMW[i] = 0;
		greyBMW[i] = 0;
		i++;

		blackBMW[i] = 0;
		greyBMW[i] = 0;
		i++;

		blackBMW[i] = 0;
		greyBMW[i] = 0;
		i++;
	} while(i < BM_WORDS);
}

void bmFillRect(s16 x, s16 y, s16 w, s16 h, u8* bm)
{
	s16 yy;

	if(!clipRect(&x, &y, &w, &h))
		return;

	for(yy = y; yy < y + h; yy++)
		fillSpan(x, yy, w, bm, 0xff);
}

void bmDrawRect(s16 x, s16 y, s16 w, s16 h, u8* bm)
{
	s16 yy;
	s16 right;
	s16 bottom;

	if(!clipRect(&x, &y, &w, &h))
		return;

	right = x + w - 1;
	bottom = y + h - 1;

	fillSpan(x, y, w, bm, 0xff);

	if(bottom != y)
		fillSpan(x, bottom, w, bm, 0xff);

	if(h <= 2)
		return;

	for(yy = y + 1; yy < bottom; yy++)
	{
		setPixel(x, yy, bm);

		if(right != x)
			setPixel(right, yy, bm);
	}
}

void bmClearRect(s16 x, s16 y, s16 w, s16 h, u8* bm)
{
	s16 yy;

	if(!clipRect(&x, &y, &w, &h))
		return;

	for(yy = y; yy < y + h; yy++)
		fillSpan(x, yy, w, bm, 0);
}

void bmFillPattern(s16 x, s16 y, s16 w, s16 h, u8* bm)
{
	s16 yy;

	if(!clipRect(&x, &y, &w, &h))
		return;

	for(yy = y; yy < y + h; yy++)
		patternSpan(x, yy, w, bm);
}

void bmDrawLine(s16 start_x, s16 start_y, s16 end_x, s16 end_y, u8* bm)
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
			setPixel(start_x, start_y, bm);

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
