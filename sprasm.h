#ifndef SPRASM_H
#define SPRASM_H

#include "fp_types.h"

/*  The row loop of a scaled sprite (drawProjectedSprite in sprite.c), which
    sets this up once a sprite and calls spriteDrawRows. It is assembler
    (sprasm.a) so the pixel loop's state lives in registers: as C, TopSpeed
    kept every one of its values on the stack and a pixel cost ~20us. The
    C twin, pc/src/sprasm_pc.c, is the reference it is held to.

    One global block rather than a pointer, so the assembler reaches every
    field by absolute address and needs no register for it. All two-byte
    members, offsets 2n: the assembler's offsets are written against them. */
typedef struct spriterows_t
{
	const u8* frame;	/*  0  the cached frame: pixels, then a span byte per row */
	s16* groupX;		/*  2  17 entries the callee fills: the destination x of each source group edge */
	u8* dstRow;		/*  4  byte 0 of the first row in the black plane, advanced a row at a time */
	u16 rows;		/*  6  destination rows left */
	u16 sourceYAcc;		/*  8  the Y interpolant, 8.8: its high byte is the source row */
	u16 sourceYStep;	/* 10 */
	s16 xStart;		/* 12  the span after every clip, half open */
	s16 xEnd;		/* 14 */
	s16 left;		/* 16  the sprite's left edge, unclipped */
	u16 sourceXStep;	/* 18 */
	u16 mirrored;		/* 20 */
	u16 occluded;		/* 22  spriteColVisible holds the visible columns */
	s16 accBase;		/* 24  scaled: the X interpolant at the frame's left edge, 0 or (mirrored) 16383 */
	s16 advance;		/* 26  scaled: its step per pixel, sourceXStep or (mirrored) its negative */
	s16 firstGroup;		/* 28  unscaled: the first 4 pixel group the clip allows */
	s16 lastGroup;		/* 30  unscaled: the last */
	s16 leftGroup;		/* 32  unscaled: left / 4, the screen group source group 0 lands on */
	u16 work[4];		/* 34  the assembler's own, per row */
} spriterows_t;

extern spriterows_t spriteRows;

/*  Per destination byte, the pixels whose wall column the sprite is in front
    of: a nibble per four pixel column. Valid when spriteRows.occluded. */
extern u8 spriteColVisible[30];

/*  An unscaled sprite's rows (drawSprite, the weapon overlay), through the
    same block: frame, dstRow, rows, sourceYAcc (the first source row in its
    high byte), firstGroup, lastGroup and leftGroup, every x a multiple of 4.
    The source is read in order, a byte of four pixels at a time, with no
    interpolant; the row's span byte trims it to its opaque groups as in
    spriteDrawRows. */
#pragma save, call(reg_saved=>(si,di,es,ds,st1,st2))
void spriteDrawRows(void);
void spriteBlitRows(void);
#pragma restore

#endif
