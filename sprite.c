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

static u8 spriteLoadBuffer[SPRITE_BYTES];
static HANDLE spriteSegs[SPRITE_MAX_SPRITES];
static u8 spriteFrameCounts[SPRITE_MAX_SPRITES];
static u8 spriteCache[SPRITE_CACHE_FRAMES * SPRITE_BYTES];
static sprite_cache_entry_t spriteCacheEntries[SPRITE_CACHE_FRAMES];
static u16 spriteCacheRand = 0xace1;

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

static const u8* getSpriteFrame(const u8 spriteId)
{
	u8 slot;
	u8 spriteNum = (spriteId >> 3) & SPRITE_NUM_MASK;
	u8 frameNum = spriteId & SPRITE_FRAME_MASK;
	u8 frameCount = spriteFrameCounts[spriteNum];
	u8 cacheSpriteId;
	HANDLE segHandle = spriteSegs[spriteNum];

	if(segHandle <= 0 || frameCount == 0)
		return &testPatternSprite[0];

	while(frameNum >= frameCount)
		frameNum -= frameCount;

	cacheSpriteId = (spriteNum << 3) | frameNum;

	for(slot = 0; slot < SPRITE_CACHE_FRAMES; slot++)
	{
		if(spriteCacheEntries[slot].valid && spriteCacheEntries[slot].spriteId == cacheSpriteId)
			return cacheFramePtr(slot);
	}

	slot = chooseCacheSlot();
	p_sgcopyfr(segHandle, ((u32)frameNum) * SPRITE_BYTES, cacheFramePtr(slot), SPRITE_BYTES);

	spriteCacheEntries[slot].spriteId = cacheSpriteId;
	spriteCacheEntries[slot].valid = TRUE;

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
	s16 xStart;
	s16 xEnd;
	s16 top;
	s16 bottom;
	s16 yStart;
	s16 yEnd;
	s16 sourceXStep;
	s16 sourceYStep;
	s16 sourceXAcc;
	const u8* spriteData;

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

	sourceXStep = (s16)((SPRITE_SIZE << SPRITE_SCALE_BITS) / width);
	sourceYStep = (s16)((SPRITE_SIZE << SPRITE_SCALE_BITS) / height);
	sourceXAcc = (s16)((s32)(xStart - left) * sourceXStep);
	spriteData = getSpriteFrame(spriteHit->spriteId);

	for(x = xStart; x < xEnd; x++)
	{
		u16 sourceX = sourceXAcc >> SPRITE_SCALE_BITS;
		const u8* sourceCol;
		u16 offset;
		u8 mask;
		u8 keepMask;
		s16 sourceYAcc;
		s16 y;

		sourceCol = spriteData + (sourceX << 4);
		offset = (yStart << 5) + (x >> 3);
		mask = 1 << (x & 7);
		keepMask = ~mask;
		sourceYAcc = (s16)((s32)(yStart - top) * sourceYStep);

		for(y = yStart; y < yEnd; y++)
		{
			u16 sourceY = sourceYAcc >> SPRITE_SCALE_BITS;
			u8 packed;
			u8 pix;

			packed = sourceCol[sourceY >> 2];
			pix = (packed >> ((sourceY & 3) << 1)) & 3;

			switch(pix)
			{
				case SPR_GREY:
					blackBm[offset] &= keepMask;
					greyBm[offset] |= mask;
					break;
				case SPR_BLACK:
					blackBm[offset] |= mask;
					greyBm[offset] &= keepMask;
					break;
				case SPR_WHITE:
					blackBm[offset] &= keepMask;
					greyBm[offset] &= keepMask;
					break;
			}

			sourceYAcc += sourceYStep;
			offset += SCREEN_ROW_BYTES;
		}

		sourceXAcc += sourceXStep;
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
	s16 x;
	const u8* spriteData;

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

	spriteData = getSpriteFrame(spriteId);

	for(x = xStart; x < xEnd; x++)
	{
		const u8* sourceCol;
		u16 offset;
		u8 mask;
		u8 keepMask;
		s16 sourceY;
		s16 yPos;

		sourceCol = spriteData + ((x - left) << 4);
		offset = (yStart << 5) + (x >> 3);
		mask = 1 << (x & 7);
		keepMask = ~mask;
		sourceY = yStart - top;

		for(yPos = yStart; yPos < yEnd; yPos++)
		{
			u8 packed;
			u8 pix;

			packed = sourceCol[sourceY >> 2];
			pix = (packed >> ((sourceY & 3) << 1)) & 3;

			switch(pix)
			{
				case SPR_GREY:
					blackBm[offset] &= keepMask;
					greyBm[offset] |= mask;
					break;
				case SPR_BLACK:
					blackBm[offset] |= mask;
					greyBm[offset] &= keepMask;
					break;
				case SPR_WHITE:
					blackBm[offset] &= keepMask;
					greyBm[offset] &= keepMask;
					break;
			}

			sourceY++;
			offset += SCREEN_ROW_BYTES;
		}
	}
}
