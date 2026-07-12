#include <plib.h>
#include <wlib.h>

#include "psion3d.h"
#include "bitmap.h"
#include "sprite.h"

#include "debug.h"


#define SPRITE_SIZE 64
#define SPRITE_COL_BYTES 16
#define SPRITE_BYTES (SPRITE_SIZE * SPRITE_COL_BYTES)
#define SPRITE_MAX_FRAMES 8
#define SPRITE_FRAME_PARAS (SPRITE_BYTES / 16)
#define SPRITE_SCALE_BITS 8
#define SPRITE_MAX_SPRITES 32
#define SPRITE_NUM_MASK 0x1f
#define SPRITE_FRAME_MASK 0x07
#define SPRITE_CACHE_FRAMES 8
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

#include "tst_spr.h"


typedef struct sprite_cache_entry_t
{
	u8 spriteId;
	u8 valid;
} sprite_cache_entry_t;

typedef struct sprite_bounds_t
{
	u8 left;
	u8 top;
	u8 right;
	u8 bottom;
} sprite_bounds_t;

static u8 spriteLoadBuffer[SPRITE_BYTES];
static HANDLE spriteSegs[SPRITE_MAX_SPRITES];
static u8 spriteFrameCounts[SPRITE_MAX_SPRITES];
static u8 spriteCache[SPRITE_CACHE_FRAMES * SPRITE_BYTES];
static u8 testPatternCache[SPRITE_BYTES];
static u8 spriteOpaqueMask[256];
static u8 spriteBlackMask[256];
static u8 spriteGreyMask[256];
static sprite_cache_entry_t spriteCacheEntries[SPRITE_CACHE_FRAMES];
static sprite_bounds_t spriteCacheBounds[SPRITE_CACHE_FRAMES];
static sprite_bounds_t testPatternBounds;
static u16 spriteCacheRand = 0xace1;
static u8 testPatternCached = FALSE;
static u8 spriteMasksReady = FALSE;

static void prepareSpriteMasks()
{
	u16 packed;

	for(packed = 0; packed < 256; packed++)
	{
		u8 i;

		for(i = 0; i < 4; i++)
		{
			u8 pix = (packed >> (i << 1)) & 3;
			u8 mask = 1 << i;

			if(pix != SPR_TRANSPARENT)
				spriteOpaqueMask[packed] |= mask;

			if(pix == SPR_BLACK)
				spriteBlackMask[packed] |= mask;
			else if(pix == SPR_GREY)
				spriteGreyMask[packed] |= mask;
		}
	}

	spriteMasksReady = TRUE;
}

/* Cached sprites are transposed from file-order columns to packed rows. */
static void prepareSpriteFrame(const u8* source, u8* dest, sprite_bounds_t* bounds)
{
	u16 y;
	u8 minX = SPRITE_SIZE;
	u8 minY = SPRITE_SIZE;
	u8 maxX = 0;
	u8 maxY = 0;

	if(!spriteMasksReady)
		prepareSpriteMasks();

	for(y = 0; y < SPRITE_SIZE; y++)
	{
		u16 x;
		u16 destOffset = y << 4;

		for(x = 0; x < SPRITE_SIZE; x += 4)
		{
			u8 packed = 0;
			u8 i;

			for(i = 0; i < 4; i++)
			{
				const u8* sourceCol = source + ((x + i) << 4);
				u8 pix = (sourceCol[y >> 2] >> ((y & 3) << 1)) & 3;

				packed |= pix << (i << 1);
			}

			dest[destOffset + (x >> 2)] = packed;

			if(packed != 0)
			{
				if(x < minX)
					minX = x;
				if(x + 4 > maxX)
					maxX = x + 4;
				if(y < minY)
					minY = y;
				if(y + 1 > maxY)
					maxY = y + 1;
			}
		}
	}

	if(minX == SPRITE_SIZE)
	{
		bounds->left = 0;
		bounds->top = 0;
		bounds->right = 0;
		bounds->bottom = 0;
		return;
	}

	bounds->left = minX;
	bounds->top = minY;
	bounds->right = maxX;
	bounds->bottom = maxY;
}

static u8 randomCacheSlot()
{
	spriteCacheRand = (u16)(spriteCacheRand * 17 + 43);

	return (u8)((spriteCacheRand >> 8) & (SPRITE_CACHE_FRAMES - 1));
}

static u8 chooseCacheSlot()
{
	u8 slot;

	for(slot = 0; slot < SPRITE_CACHE_FRAMES; slot++)
	{
		if(!spriteCacheEntries[slot].valid)
			return slot;
	}

	return randomCacheSlot();
}

static u8* cacheFramePtr(const u8 slot)
{
	return &spriteCache[((u16)slot) * SPRITE_BYTES];
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
	HANDLE segHandle = spriteSegs[spriteNum];

	if(segHandle <= 0 || frameCount == 0)
	{
		if(!testPatternCached)
		{
			prepareSpriteFrame(&testPatternSprite[0], &testPatternCache[0], &testPatternBounds);
			testPatternCached = TRUE;
		}

		*bounds = testPatternBounds;
		return &testPatternCache[0];
	}

	while(frameNum >= frameCount)
		frameNum -= frameCount;

	cacheSpriteId = (spriteNum << 3) | frameNum;

	for(slot = 0; slot < SPRITE_CACHE_FRAMES; slot++)
	{
		if(spriteCacheEntries[slot].valid && spriteCacheEntries[slot].spriteId == cacheSpriteId)
		{
			*bounds = spriteCacheBounds[slot];
			return cacheFramePtr(slot);
		}
	}

	slot = chooseCacheSlot();
	p_sgcopyfr(segHandle, ((u32)frameNum) * SPRITE_BYTES, &spriteLoadBuffer[0], SPRITE_BYTES);
	prepareSpriteFrame(&spriteLoadBuffer[0], cacheFramePtr(slot), &spriteCacheBounds[slot]);

	spriteCacheEntries[slot].spriteId = cacheSpriteId;
	spriteCacheEntries[slot].valid = TRUE;

	*bounds = spriteCacheBounds[slot];
	return cacheFramePtr(slot);
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

		bytesRead = p_read(fileHandle, &spriteLoadBuffer[0], SPRITE_BYTES);

		p_close(fileHandle);

		if(bytesRead != SPRITE_BYTES)
		{
			p_sgclose(segHandle);
			return 0;
		}

		p_sgcopyto(segHandle, ((u32)frame) * SPRITE_BYTES, &spriteLoadBuffer[0], SPRITE_BYTES);
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

	return TRUE;
}

void drawProjectedSprite(const spritehit_t* spriteHit)
{
	s16 height = spriteHit->spriteHeight;
	s16 width;
	s16 left;
	s16 right;
	s16 x;
	s16 y;
	s16 xStart;
	s16 xEnd;
	s16 top;
	s16 bottom;
	s16 yStart;
	s16 yEnd;
	s16 sourceXStep;
	s16 sourceYStep;
	s16 sourceYAcc;
	s16 boundOffset;
	const u8* spriteData;
	sprite_bounds_t bounds;

	if(height <= 0)
		return;

	width = height;

	if(width < 1)
		width = 1;

	left = (spriteHit->spanX << 2) + 2 - (width >> 1);
	right = left + width;
	xStart = left;
	xEnd = right;

	if(xStart < 0)
		xStart = 0;

	if(xEnd > SCREEN_WIDTH)
		xEnd = SCREEN_WIDTH;

	if(xStart >= xEnd)
		return;

	top = 80 - (height >> 1);
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

	sourceXStep = (s16)((SPRITE_SIZE << SPRITE_SCALE_BITS) / width);
	sourceYStep = (s16)((SPRITE_SIZE << SPRITE_SCALE_BITS) / height);

	boundOffset = (s16)((((s32)bounds.left << SPRITE_SCALE_BITS) +
		sourceXStep - 1) / sourceXStep);
	if(xStart < left + boundOffset)
		xStart = left + boundOffset;

	boundOffset = (s16)((((s32)bounds.right << SPRITE_SCALE_BITS) +
		sourceXStep - 1) / sourceXStep);
	if(xEnd > left + boundOffset)
		xEnd = left + boundOffset;

	boundOffset = (s16)((((s32)bounds.top << SPRITE_SCALE_BITS) +
		sourceYStep - 1) / sourceYStep);
	if(yStart < top + boundOffset)
		yStart = top + boundOffset;

	boundOffset = (s16)((((s32)bounds.bottom << SPRITE_SCALE_BITS) +
		sourceYStep - 1) / sourceYStep);
	if(yEnd > top + boundOffset)
		yEnd = top + boundOffset;

	if(xStart >= xEnd || yStart >= yEnd)
		return;

	sourceYAcc = (s16)((s32)(yStart - top) * sourceYStep);

	for(y = yStart; y < yEnd; y++)
	{
		u16 sourceY = sourceYAcc >> SPRITE_SCALE_BITS;
		const u8* sourceRow = spriteData + (sourceY << 4);
		s16 sourceXAcc = (s16)((s32)(xStart - left) * sourceXStep);
		u16 offset = (y << 5) + (xStart >> 3);

		x = xStart;
		while(x < xEnd)
		{
			u8 opaqueMask = 0;
			u8 blackMask = 0;
			u8 greyMask = 0;
			u8 count = 8 - (x & 7);
			u8 i;

			if(count > xEnd - x)
				count = xEnd - x;

			for(i = 0; i < count; i++)
			{
				u16 sourceX = sourceXAcc >> SPRITE_SCALE_BITS;
				u8 packed = sourceRow[sourceX >> 2];
				u8 pix = (packed >> ((sourceX & 3) << 1)) & 3;
				u8 mask = 1 << ((x + i) & 7);

				if(pix != SPR_TRANSPARENT)
				{
					opaqueMask |= mask;

					if(pix == SPR_BLACK)
						blackMask |= mask;
					else if(pix == SPR_GREY)
						greyMask |= mask;
				}

				sourceXAcc += sourceXStep;
			}

			blackBm[offset] = (blackBm[offset] & ~opaqueMask) | blackMask;
			greyBm[offset] = (greyBm[offset] & ~opaqueMask) | greyMask;
			x += count;
			offset++;
		}

		sourceYAcc += sourceYStep;
	}
}

void drawSprite(u8 spanX, u8 y, u8 spriteId)
{
	s16 left = spanX << 2;
	s16 top = y;
	s16 right = left + SPRITE_SIZE;
	s16 bottom = top + SPRITE_SIZE;
	s16 xStart = left;
	s16 xEnd = right;
	s16 yStart = top;
	s16 yEnd = bottom;
	s16 yPos;
	const u8* spriteData;
	sprite_bounds_t bounds;

	if(xStart < 0)
		xStart = 0;

	if(xEnd > SCREEN_WIDTH)
		xEnd = SCREEN_WIDTH;

	if(xStart >= xEnd)
		return;

	if(yStart < 0)
		yStart = 0;

	if(yEnd > SCREEN_HEIGHT)
		yEnd = SCREEN_HEIGHT;

	if(yStart >= yEnd)
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

	for(yPos = yStart; yPos < yEnd; yPos++)
	{
		const u8* sourceRow = spriteData + ((yPos - top) << 4);
		u16 offset = (yPos << 5) + (xStart >> 3);
		s16 x = xStart;

		while(x < xEnd)
		{
			u8 packed = sourceRow[(x - left) >> 2];
			u8 shift = x & 4;
			u8 opaqueMask = spriteOpaqueMask[packed] << shift;
			u8 blackMask = spriteBlackMask[packed] << shift;
			u8 greyMask = spriteGreyMask[packed] << shift;

			blackBm[offset] = (blackBm[offset] & ~opaqueMask) | blackMask;
			greyBm[offset] = (greyBm[offset] & ~opaqueMask) | greyMask;
			x += 4;

			if(!(x & 7))
				offset++;
		}
	}
}
