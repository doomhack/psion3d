/*  sprasm_pc.c - portable replacement for sprasm.a.

    The device runs the scaled sprite's row loop in assembler so the pixel
    loop's state stays in registers (sprasm.h). This is the same loop in C,
    the reference the assembler is held to: per row the Y interpolant's
    source row, its span byte to skip an empty row and bound the rest, then
    the X interpolant a pixel at a time, gathered into the destination
    byte's opaque, black and grey bits and written when the byte is done. */

#include "bitmap.h"
#include "sprasm.h"

#define SPRITE_BYTES 1024
#define SPRITE_SIZE 64
#define SPRITE_SCALE_BITS 8
#define SPRITE_GREY_PLANE BM_BYTES
#define SCREEN_ROW_BYTES 32

#define SPR_TRANSPARENT 0
#define SPR_GREY 1
#define SPR_BLACK 2

void spriteDrawRows(void)
{
	spriterows_t* r = &spriteRows;
	const u8* rowSpans = r->frame + SPRITE_BYTES;
	u16 g;

	/* The destination x each source group's left edge lands on, the frame's
	   own (mirrored, group g is source group 15 - g): left plus the ceiling of
	   (4g << 8) / sourceXStep, as scaleBound has it. The assembler gets the
	   same from one divide and a carried remainder. */
	for(g = 0; g <= 16; g++)
		r->groupX[g] = (s16)(r->left + (s16)((u16)((g << 10) + r->sourceXStep - 1) / r->sourceXStep));

	for(; r->rows > 0; r->rows--, r->dstRow += SCREEN_ROW_BYTES, r->sourceYAcc = (u16)(r->sourceYAcc + r->sourceYStep))
	{
		const u16 sourceY = (u16)(r->sourceYAcc >> SPRITE_SCALE_BITS);
		const u8 span = rowSpans[sourceY];
		const u8* source = r->frame + (sourceY << 4);
		u8 first = (u8)(span >> 4);
		u8 last = (u8)(span & 0x0f);
		s16 x;
		s16 rowXEnd;
		s16 sourceXAcc;
		s16 sourceXAdvance;
		u8* dst;
		u8 bit;
		u8 opaqueMask;
		u8 blackMask;
		u8 greyMask;

		if(first > last)
			continue;

		if(r->mirrored)
		{
			u8 swap = first;

			first = (u8)(15 - last);
			last = (u8)(15 - swap);
		}

		x = r->groupX[first];
		rowXEnd = r->groupX[last + 1];

		if(x < r->xStart)
			x = r->xStart;

		if(rowXEnd > r->xEnd)
			rowXEnd = r->xEnd;

		if(x >= rowXEnd)
			continue;

		/* accBase and advance carry the mirror: 0 and +step, or 16383 and -step. */
		sourceXAcc = (s16)(r->accBase + (x - r->left) * r->advance);
		sourceXAdvance = r->advance;

		dst = r->dstRow + (x >> 3);
		bit = (u8)(1 << (x & 7));
		opaqueMask = 0;
		blackMask = 0;
		greyMask = 0;

		for(;;)
		{
			const u16 sourceX = (u16)(sourceXAcc >> SPRITE_SCALE_BITS);
			const u8 pix = (u8)((source[sourceX >> 2] >> ((sourceX & 3) << 1)) & 3);

			if(pix != SPR_TRANSPARENT)
			{
				opaqueMask |= bit;

				if(pix == SPR_BLACK)
					blackMask |= bit;
				else if(pix == SPR_GREY)
					greyMask |= bit;
			}

			sourceXAcc = (s16)(sourceXAcc + sourceXAdvance);
			x++;
			bit <<= 1;

			if(bit == 0 || x >= rowXEnd)
			{
				if(r->occluded)
				{
					const u8 visible = spriteColVisible[(u16)(dst - blackBm) & (SCREEN_ROW_BYTES - 1)];

					opaqueMask &= visible;
					blackMask &= visible;
					greyMask &= visible;
				}

				if(opaqueMask == 0xff)
				{
					dst[0] = blackMask;
					dst[SPRITE_GREY_PLANE] = greyMask;
				}
				else if(opaqueMask)
				{
					dst[0] = (u8)((dst[0] & ~opaqueMask) | blackMask);
					dst[SPRITE_GREY_PLANE] = (u8)((dst[SPRITE_GREY_PLANE] & ~opaqueMask) | greyMask);
				}

				if(x >= rowXEnd)
					break;

				dst++;
				bit = 1;
				opaqueMask = 0;
				blackMask = 0;
				greyMask = 0;
			}
		}
	}
}

/*  The unscaled rows: each row's span byte trimmed to the 4 pixel groups the
    clip allows (every x a multiple of 4), then each group's four pixels in
    order into the destination byte, which is written when it is full or the
    row ends. A sprite starting on the odd nibble fills only the top half of
    its first byte. */
void spriteBlitRows(void)
{
	spriterows_t* r = &spriteRows;
	const u8* rowSpans = r->frame + SPRITE_BYTES;

	for(; r->rows > 0; r->rows--, r->dstRow += SCREEN_ROW_BYTES, r->sourceYAcc = (u16)(r->sourceYAcc + (1 << SPRITE_SCALE_BITS)))
	{
		const u16 sourceY = (u16)(r->sourceYAcc >> SPRITE_SCALE_BITS);
		const u8 span = rowSpans[sourceY];
		s16 first = (s16)(span >> 4);
		s16 last = (s16)(span & 0x0f);
		const u8* source;
		s16 x;
		s16 g;
		u8* dst;
		u8 bit;
		u8 opaqueMask = 0;
		u8 blackMask = 0;
		u8 greyMask = 0;

		if(first > last)
			continue;

		if(first < r->firstGroup)
			first = r->firstGroup;

		if(last > r->lastGroup)
			last = r->lastGroup;

		if(first > last)
			continue;

		source = r->frame + (sourceY << 4) + first;
		/* The screen group the first drawn group lands on: its byte, and which half. */
		x = (s16)(r->leftGroup + first);
		dst = r->dstRow + (x >> 1);
		bit = (u8)((x & 1) ? 0x10 : 0x01);

		for(g = first; g <= last; g++)
		{
			u8 packed = *source++;
			u8 p;

			for(p = 0; p < 4; p++, packed >>= 2, bit <<= 1)
			{
				const u8 pix = (u8)(packed & 3);

				if(pix != SPR_TRANSPARENT)
				{
					opaqueMask |= bit;

					if(pix == SPR_BLACK)
						blackMask |= bit;
					else if(pix == SPR_GREY)
						greyMask |= bit;
				}
			}

			if(bit == 0 || g == last)
			{
				if(opaqueMask == 0xff)
				{
					dst[0] = blackMask;
					dst[SPRITE_GREY_PLANE] = greyMask;
				}
				else if(opaqueMask)
				{
					dst[0] = (u8)((dst[0] & ~opaqueMask) | blackMask);
					dst[SPRITE_GREY_PLANE] = (u8)((dst[SPRITE_GREY_PLANE] & ~opaqueMask) | greyMask);
				}

				dst++;
				bit = 1;
				opaqueMask = 0;
				blackMask = 0;
				greyMask = 0;
			}
		}
	}
}
