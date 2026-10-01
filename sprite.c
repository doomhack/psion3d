#include <plib.h>
#include <wlib.h>

#include "psion3d.h"
#include "bitmap.h"
#include "sprite.h"
#include "sprslot.h"
#include "sprasm.h"
#include "cheat.h"

#include "debug.h"


/* Frames held in near RAM. Nine because the Decorations benchmark station
   shows nine distinct frames at once - four decorations, four pickups and
   the weapon - and an LRU cache one short of a cyclic working set misses on
   every access: it was eight, and the nine segment copies a frame that
   cost measured 3.2 ms there. A slot is a frame, 1,104 bytes of DGROUP.

   SPRITES.md describes the whole pipeline, and TASKS.md task 22 has the
   measurements behind it. */
#define SPRITE_CACHE_FRAMES 9

#define SPRITE_SIZE 64
#define SPRITE_ROW_BYTES 16
#define SPRITE_BYTES (SPRITE_SIZE * SPRITE_ROW_BYTES)
#define SPRITE_HEADER_BYTES 16
#define SPRITE_MAX_FRAMES 8
/* A frame as stored in its slot's segment and in the cache: the 1,024 bytes of
   pixels from the file, one span byte per row (buildRowSpans), then the
   header's box in a paragraph of its own, so it arrives with the frame and no
   near table holds it for every frame that could be loaded. */
#define SPRITE_FRAME_BOX (SPRITE_BYTES + SPRITE_SIZE)
#define SPRITE_FRAME_BYTES (SPRITE_FRAME_BOX + 16)
#define SPRITE_FRAME_PARAS (SPRITE_FRAME_BYTES / 16)
#define SPRITE_SCALE_BITS 8
#define SPRITE_NUM_MASK 0x1f
#define SPRITE_FRAME_MASK 0x07
#define SPRITE_HEIGHT_NUM ((s16)30720)

#define SPR_TRANSPARENT 0
#define SPR_GREY 1
#define SPR_BLACK 2
#define SPR_WHITE 3

#define SPRITE_NEAR_DEPTH ((f16)32)
#define SPRITE_FAR_DEPTH ((f16)3072)


#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 160
#define SCREEN_ROW_BYTES 32

#define SPRITE_FILE_NAME_LEN 64
#define SPRITE_SEG_NAME_LEN 16



typedef struct sprite_cache_entry_t
{
	u8 spriteId;
	u8 valid;
	u16 lastUse;
} sprite_cache_entry_t;

typedef struct sprite_bounds_t
{
	u8 left;
	u8 top;
	u8 right;
	u8 bottom;
} sprite_bounds_t;

static u8 spriteLoadBuffer[SPRITE_FRAME_BYTES];
static u8 spriteHeader[SPRITE_HEADER_BYTES];
static HANDLE spriteSegs[SPRITE_SLOT_CAPACITY];
static u8 spriteFrameCounts[SPRITE_SLOT_CAPACITY];
static u8 spriteCache[SPRITE_CACHE_FRAMES * SPRITE_FRAME_BYTES];
static sprite_cache_entry_t spriteCacheEntries[SPRITE_CACHE_FRAMES];
static u16 spriteCacheClock = 0;

static void mirrorSpriteBounds(sprite_bounds_t* bounds)
{
	u8 left = bounds->left;

	bounds->left = SPRITE_SIZE - bounds->right;
	bounds->right = SPRITE_SIZE - left;
}

static u16 spriteCacheTouch()
{
	if(++spriteCacheClock == 0)
	{
		u8 slot;

		/* Clock wrapped. Flatten the ages so ordering stays meaningful. */
		for(slot = 0; slot < SPRITE_CACHE_FRAMES; slot++)
			spriteCacheEntries[slot].lastUse = 0;

		spriteCacheClock = 1;
	}

	return spriteCacheClock;
}

/* Least recently used. Random eviction thrashed here: a frame can need more
   distinct frames than there are slots, and every miss costs a segment copy. */
static u8 chooseCacheSlot()
{
	u8 slot;
	u8 oldest = 0;
	u16 oldestUse = 0xffff;

	for(slot = 0; slot < SPRITE_CACHE_FRAMES; slot++)
	{
		if(!spriteCacheEntries[slot].valid)
			return slot;

		if(spriteCacheEntries[slot].lastUse < oldestUse)
		{
			oldestUse = spriteCacheEntries[slot].lastUse;
			oldest = slot;
		}
	}

	return oldest;
}

static u8* cacheFramePtr(const u8 slot)
{
	return &spriteCache[((u16)slot) * SPRITE_FRAME_BYTES];
}

static void invalidateSpriteCache(const u8 spriteNum)
{
	u8 slot;

	for(slot = 0; slot < SPRITE_CACHE_FRAMES; slot++)
	{
		if(spriteCacheEntries[slot].valid &&
			((spriteCacheEntries[slot].spriteId >> 3) & SPRITE_NUM_MASK) == spriteNum)
			spriteCacheEntries[slot].valid = FALSE;
	}
}

static const u8* getSpriteFrame(const u8 spriteId, sprite_bounds_t* bounds)
{
	u8 slot;
	u8 spriteNum = (spriteId >> 3) & SPRITE_NUM_MASK;
	u8 frameNum = spriteId & SPRITE_FRAME_MASK;
	u8 frameCount = spriteFrameCounts[spriteNum];
	u8 cacheSpriteId;
	u8* frame;
	HANDLE segHandle = spriteSegs[spriteNum];

	if(segHandle <= 0 || frameCount == 0)
	{
		/* Slot never loaded. Signal an empty frame; both callers bail on this. */
		bounds->right = 0;
		return NULL;
	}

	while(frameNum >= frameCount)
		frameNum -= frameCount;

	cacheSpriteId = (spriteNum << 3) | frameNum;

	for(slot = 0; slot < SPRITE_CACHE_FRAMES; slot++)
	{
		if(spriteCacheEntries[slot].valid && spriteCacheEntries[slot].spriteId == cacheSpriteId)
			break;
	}

	if(slot == SPRITE_CACHE_FRAMES)
	{
		slot = chooseCacheSlot();
		p_sgcopyfr(segHandle, ((u32)frameNum) * SPRITE_FRAME_BYTES, cacheFramePtr(slot), SPRITE_FRAME_BYTES);

		spriteCacheEntries[slot].spriteId = cacheSpriteId;
		spriteCacheEntries[slot].valid = TRUE;
	}

	spriteCacheEntries[slot].lastUse = spriteCacheTouch();
	frame = cacheFramePtr(slot);
	*bounds = *(const sprite_bounds_t*)(frame + SPRITE_FRAME_BOX);

	return frame;
}

/* The header's box, validated, into bounds. Its band bytes are not read: the
   span bytes built at load (buildRowSpans) are per row and replaced them. */
static u16 readSpriteHeader(VOID* fileHandle, sprite_bounds_t* bounds)
{
	INT bytesRead = p_read(fileHandle, &spriteHeader[0], SPRITE_HEADER_BYTES);

	if(bytesRead != SPRITE_HEADER_BYTES ||
		spriteHeader[0] != 'S' || spriteHeader[1] != 'P' ||
		spriteHeader[2] != 'R' || spriteHeader[3] != 1)
		return FALSE;

	bounds->left = spriteHeader[4];
	bounds->top = spriteHeader[5];
	bounds->right = spriteHeader[6];
	bounds->bottom = spriteHeader[7];

	if(bounds->left > bounds->right || bounds->right > SPRITE_SIZE ||
		bounds->top > bounds->bottom || bounds->bottom > SPRITE_SIZE)
		return FALSE;

	return TRUE;
}

/* After a frame's pixels, one byte per row: the row's first and last 4 pixel
   group with anything opaque in it, as (first << 4) | last - the encoding the
   header's 8 row bands use - or 0xF0 for a row with nothing opaque. A group is
   one source byte, so this is a scan for the row's first and last non-zero
   byte. Built here, once a frame, rather than stored in the .spr files. */
static void buildRowSpans(u8* frame)
{
	u8* span = frame + SPRITE_BYTES;
	const u8* row = frame;
	u8 y;

	for(y = 0; y < SPRITE_SIZE; y++, row += SPRITE_ROW_BYTES)
	{
		u8 first = 0;
		u8 last = SPRITE_ROW_BYTES - 1;

		while(first < SPRITE_ROW_BYTES && row[first] == 0)
			first++;

		if(first == SPRITE_ROW_BYTES)
		{
			span[y] = 0xf0;
			continue;
		}

		while(row[last] == 0)
			last--;

		span[y] = (u8)((first << 4) | last);
	}
}

HANDLE loadSprite(TEXT* baseName, u8 id)
{
	TEXT fileName[SPRITE_FILE_NAME_LEN];
	TEXT segName[SPRITE_SEG_NAME_LEN];
	VOID* fileHandle;
	HANDLE segHandle;
	u16 frame;
	u16 frameCount;
	u8 spriteNum = id & SPRITE_NUM_MASK;
	/* The box lands where the frame keeps it, after the span bytes. */
	sprite_bounds_t* loadBounds = (sprite_bounds_t*)&spriteLoadBuffer[SPRITE_FRAME_BOX];

	/* A slot loaded for an earlier mission still holds its segment. Release
	   it first: the new one is created under the same SPR<n> name, and a
	   leaked segment per slot per mission would run the machine dry. */
	if(spriteSegs[spriteNum] > 0)
	{
		p_sgclose(spriteSegs[spriteNum]);
		spriteSegs[spriteNum] = 0;
		spriteFrameCounts[spriteNum] = 0;
		invalidateSpriteCache(spriteNum);
	}

	for(frame = 0; frame < SPRITE_MAX_FRAMES; frame++)
	{
		INT bytesRead;
		s8 extra;

		p_atos(&fileName[0], "LOC::M:\\IMG\\SPR\\%s%d.spr", baseName, frame);

		if(p_open(&fileHandle, &fileName[0], P_FOPEN | P_FSTREAM) != 0)
		{
			if(frame == 0)
				return 0;

			break;
		}

		if(!readSpriteHeader(fileHandle, loadBounds))
		{
			p_close(fileHandle);
			return 0;
		}

		bytesRead = p_read(fileHandle, &spriteLoadBuffer[0], SPRITE_BYTES);

		if(bytesRead != SPRITE_BYTES)
		{
			p_close(fileHandle);
			return 0;
		}

		bytesRead = p_read(fileHandle, &extra, 1);

		if(bytesRead != E_FILE_EOF)
		{
			p_close(fileHandle);
			return 0;
		}

		p_close(fileHandle);
	}

	if(frame == 0)
		return 0;

	frameCount = frame;

	p_atos(&segName[0], "SPR%d", spriteNum);

	segHandle = p_sgcreate(&segName[0], frameCount * SPRITE_FRAME_PARAS, E_SEGMENT_HIGH);

	if(segHandle <= 0)
		return 0;

	for(frame = 0; frame < frameCount; frame++)
	{
		INT bytesRead;

		p_atos(&fileName[0], "LOC::M:\\IMG\\SPR\\%s%d.spr", baseName, frame);

		if(p_open(&fileHandle, &fileName[0], P_FOPEN | P_FSTREAM) != 0)
		{
			p_sgclose(segHandle);
			return 0;
		}

		if(!readSpriteHeader(fileHandle, loadBounds))
		{
			p_close(fileHandle);
			p_sgclose(segHandle);
			return 0;
		}

		bytesRead = p_read(fileHandle, &spriteLoadBuffer[0], SPRITE_BYTES);

		p_close(fileHandle);

		if(bytesRead != SPRITE_BYTES)
		{
			p_sgclose(segHandle);
			return 0;
		}

		buildRowSpans(&spriteLoadBuffer[0]);
		p_sgcopyto(segHandle, ((u32)frame) * SPRITE_FRAME_BYTES, &spriteLoadBuffer[0], SPRITE_FRAME_BYTES);
	}

	invalidateSpriteCache(spriteNum);
	spriteFrameCounts[spriteNum] = (u8)frameCount;
	spriteSegs[spriteNum] = segHandle;

	return segHandle;
}

u16 projectSprite(const f16 x, const f16 y, spritehit_t* hit, const f16 f_viewCos, const f16 f_viewSin)
{
	f16 f_rx = x - player.pos.x;
	f16 f_ry = y - player.pos.y;

	f16 f_depth =
		fpmul(f_rx, f_viewCos) +
		fpmul(f_ry, f_viewSin);

	f16 f_side;
	s16 spanx;

	if(f_depth < SPRITE_NEAR_DEPTH)
		return FALSE;

	if(f_depth > SPRITE_FAR_DEPTH)
		return FALSE;

	f_side =
		-fpmul(f_rx, f_viewSin) +
		 fpmul(f_ry, f_viewCos);

	/* A sprite is only on screen while |f_side| is under 0.577 of f_depth, so
	   rejecting at 1.0 discards nothing that could be drawn. What it does do is
	   hold the ratio below 1.0 before it reaches the fpmul below, which returns
	   the product's bits 8..23 with no clamp of its own. Past a ratio of about
	   2.46 that product wraps, and the wrapped span lands back inside the
	   accepted 0..59 range often enough to draw a sprite that is actually
	   beside or behind the player at an arbitrary screen column - which reads
	   as the sprite warping across the screen as you close on it. */
	if(f_side >= f_depth || f_side <= -f_depth)
		return FALSE;

	/* The mirror cheat has turned the rays (drawSetMirror): what is to the
	   right is drawn on the left. */
	if(cheatActive & CHEAT_MIRROR)
		f_side = -f_side;

	spanx = 30 + fp2int(
		fpmul(
			fpdiv(f_side, f_depth),
			int2fp(52)
		)
	);

	if(spanx < 0 || spanx >= 60)
		return FALSE;

	hit->spriteHeight = SPRITE_HEIGHT_NUM / f_depth;
	hit->f_spriteDist = f_depth;
	hit->spanX = spanx;

	/* Centred unless the caller says otherwise, so only the impact marker has
	   to think about this. */
	hit->offsetX = 0;
	hit->offsetY = 0;

	return TRUE;
}

/* The grey plane follows the black one in screenBm (bitmap.c), so one pointer
   reaches both and the blit loops need not reload blackBm and greyBm - which
   TopSpeed does on every byte, since a store through either might move them. */
#define SPRITE_GREY_PLANE BM_BYTES

/* The row loop's block and the visible columns it reads, declared in sprasm.h:
   the assembler reaches both by name. spriteColVisible is built only for a
   sprite with a nearer wall inside its span, not just at its edges. */
spriterows_t spriteRows;
u8 spriteColVisible[SCREEN_WIDTH / 8];

/* Ceiling of (value << SPRITE_SCALE_BITS) / step. Both operands fit in 16 bits:
   value is at most SPRITE_SIZE and step at most SPRITE_SIZE << SPRITE_SCALE_BITS,
   so this stays a native 16 bit divide instead of a called 32 bit one. */
static s16 scaleBound(const u16 value, const u16 step)
{
	return (s16)((u16)((value << SPRITE_SCALE_BITS) + step - 1) / step);
}

/* Draws a scaled sprite straight from its source pixels: a Y interpolant picks
   each destination row's source row, whose span byte says which of its 4 pixel
   groups hold anything (an empty row is skipped outright); an X interpolant
   then steps across that span a destination pixel at a time, and the pixels
   are gathered into the destination byte's opaque, black and grey bits and
   written to both planes when the byte is complete. Nothing is decoded ahead:
   no column table, no row buffers, and a source row drawn on several
   destination rows is read again for each. The interpolants, and the edges
   each group lands on, are computed as before, so the pixels drawn are too. */
void drawProjectedSprite(const spritehit_t* spriteHit, const f16* f_wallDepth)
{
	s16 height = spriteHit->spriteHeight;
	s16 width;
	s16 left;
	s16 right;
	s16 xStart;
	s16 xEnd;
	s16 top;
	s16 bottom;
	s16 yStart;
	s16 yEnd;
	s16 sourceXStep;
	s16 sourceYStep;
	s16 boundOffset;
	s16 groupX[SPRITE_ROW_BYTES + 1];
	const u8* spriteData;
	sprite_bounds_t bounds;
	const u8 mirrored = spriteHit->mirrored;
	u16 firstCol;
	u16 lastCol;
	u16 col;
	u8 occluded;

	if(height <= 0)
		return;

	width = height;

	if(width < 1)
		width = 1;

	left = (spriteHit->spanX << 2) + 2 - (width >> 1) + spriteHit->offsetX;
	right = left + width;
	xStart = left;
	xEnd = right;

	if(xStart < 0)
		xStart = 0;

	if(xEnd > SCREEN_WIDTH)
		xEnd = SCREEN_WIDTH;

	if(xStart >= xEnd)
		return;

	top = 80 - (height >> 1) + spriteHit->offsetY;
	bottom = top + height;
	yStart = top;
	yEnd = bottom;

	if(yStart < 0)
		yStart = 0;

	if(yEnd > SCREEN_HEIGHT)
		yEnd = SCREEN_HEIGHT;

	if(yStart >= yEnd)
		return;

	spriteData = getSpriteFrame(spriteHit->spriteId, &bounds);

	if(bounds.right == 0)
		return;

	if(mirrored)
		mirrorSpriteBounds(&bounds);

	sourceXStep = (s16)((SPRITE_SIZE << SPRITE_SCALE_BITS) / width);
	sourceYStep = (s16)((SPRITE_SIZE << SPRITE_SCALE_BITS) / height);

	if(sourceXStep < 1)
		sourceXStep = 1;

	if(sourceYStep < 1)
		sourceYStep = 1;

	boundOffset = scaleBound(bounds.left, (u16)sourceXStep);
	if(xStart < left + boundOffset)
		xStart = left + boundOffset;

	boundOffset = scaleBound(bounds.right, (u16)sourceXStep);
	if(xEnd > left + boundOffset)
		xEnd = left + boundOffset;

	boundOffset = scaleBound(bounds.top, (u16)sourceYStep);
	if(yStart < top + boundOffset)
		yStart = top + boundOffset;

	boundOffset = scaleBound(bounds.bottom, (u16)sourceYStep);
	if(yEnd > top + boundOffset)
		yEnd = top + boundOffset;

	if(xStart >= xEnd || yStart >= yEnd)
		return;

	/* Clip against the walls a column at a time. The caller has already tested
	   the centre column - a sprite whose centre is behind a wall is not drawn,
	   and cannot be shot either - so this only hides the parts a nearer wall
	   covers, which were painted over that wall's edge. Hidden columns at the
	   ends just narrow the span; any inside it are masked off as each byte is
	   written. f_wallDepth is distance along each ray and f_spriteDist is
	   perpendicular, the same comparison the centre test makes. */
	firstCol = (u16)(xStart >> 2);
	lastCol = (u16)((xEnd - 1) >> 2);

	while(firstCol <= lastCol && spriteHit->f_spriteDist >= f_wallDepth[firstCol])
		firstCol++;

	while(lastCol > firstCol && spriteHit->f_spriteDist >= f_wallDepth[lastCol])
		lastCol--;

	if(firstCol > lastCol)
		return;

	if(xStart < (s16)(firstCol << 2))
		xStart = (s16)(firstCol << 2);

	if(xEnd > (s16)((lastCol + 1) << 2))
		xEnd = (s16)((lastCol + 1) << 2);

	occluded = FALSE;

	for(col = firstCol + 1; col < lastCol; col++)
	{
		if(spriteHit->f_spriteDist >= f_wallDepth[col])
		{
			occluded = TRUE;
			break;
		}
	}

	if(occluded)
	{
		for(col = firstCol; col <= lastCol; col++)
		{
			u8 nibble = (u8)((col & 1) ? 0xf0 : 0x0f);

			if(col == firstCol || !(col & 1))
				spriteColVisible[col >> 1] = 0;

			if(spriteHit->f_spriteDist < f_wallDepth[col])
				spriteColVisible[col >> 1] |= nibble;
		}
	}

	/* (yStart - top) is less than height and sourceYStep is
	   (SPRITE_SIZE << SPRITE_SCALE_BITS) / height, so the product always fits in
	   16 bits and needs no 32 bit multiply. The same holds on the x axis. The
	   rows themselves are sprasm.a's. */
	spriteRows.frame = spriteData;
	spriteRows.groupX = groupX;
	spriteRows.dstRow = blackBm + (yStart << 5);
	spriteRows.rows = (u16)(yEnd - yStart);
	spriteRows.sourceYAcc = (u16)((yStart - top) * sourceYStep);
	spriteRows.sourceYStep = (u16)sourceYStep;
	spriteRows.xStart = xStart;
	spriteRows.xEnd = xEnd;
	spriteRows.left = left;
	spriteRows.sourceXStep = (u16)sourceXStep;
	spriteRows.mirrored = mirrored;
	spriteRows.occluded = occluded;
	spriteRows.accBase = mirrored ? ((SPRITE_SIZE << SPRITE_SCALE_BITS) - 1) : 0;
	spriteRows.advance = mirrored ? -sourceXStep : sourceXStep;

	spriteDrawRows();
}
/* An unscaled sprite - the weapon overlay - at spanX * 4, y. It shares the
   scaled path's frame data and its writes, but needs no interpolant: the
   source is read in order, four pixels a byte, by spriteBlitRows (sprasm.a).
   Every clip edge here - the screen, the frame's box, the frame's own left -
   is a multiple of 4, so clipping by whole 4 pixel groups is exact. */
void drawSprite(u8 spanX, u8 y, u8 spriteId)
{
	const s16 left = spanX << 2;
	const s16 top = y;
	s16 xStart = left;
	s16 xEnd = left + SPRITE_SIZE;
	s16 yStart = top;
	s16 yEnd = top + SPRITE_SIZE;
	const u8* spriteData;
	sprite_bounds_t bounds;

	if(xStart < 0)
		xStart = 0;

	if(xEnd > SCREEN_WIDTH)
		xEnd = SCREEN_WIDTH;

	if(yStart < 0)
		yStart = 0;

	if(yEnd > SCREEN_HEIGHT)
		yEnd = SCREEN_HEIGHT;

	if(xStart >= xEnd || yStart >= yEnd)
		return;

	spriteData = getSpriteFrame(spriteId, &bounds);

	if(bounds.right == 0)
		return;

	if(xStart < left + bounds.left)
		xStart = left + bounds.left;

	if(xEnd > left + bounds.right)
		xEnd = left + bounds.right;

	if(yStart < top + bounds.top)
		yStart = top + bounds.top;

	if(yEnd > top + bounds.bottom)
		yEnd = top + bounds.bottom;

	if(xStart >= xEnd || yStart >= yEnd)
		return;

	spriteRows.frame = spriteData;
	spriteRows.dstRow = blackBm + (yStart << 5);
	spriteRows.rows = (u16)(yEnd - yStart);
	spriteRows.sourceYAcc = (u16)((yStart - top) << SPRITE_SCALE_BITS);
	spriteRows.xStart = xStart;
	spriteRows.xEnd = xEnd;
	spriteRows.left = left;
	spriteRows.firstGroup = (s16)((xStart - left) >> 2);
	spriteRows.lastGroup = (s16)(((xEnd - left) >> 2) - 1);
	spriteRows.leftGroup = (s16)(left >> 2);

	spriteBlitRows();
}