#include <plib.h>
#include <wlib.h>

#include "psion3d.h"
#include "bitmap.h"
#include "sprite.h"
#include "sprslot.h"
#include "game_map.h"
#include "enemy.h"
#include "walls.h"

typedef struct markedsprite_t
{
	u8 x, y;
} markedsprite_t;

#define WALL_HEIGHT_NUM ((s16)30720)

/* Largest per cell step the s16 side distance accumulator can take and still
   not wrap over the map's ~90 cell diagonal. A ray with a bigger delta is
   within about a degree of axis-parallel; those take the fpdiv path below. */
#define DDA_SAFE_DELTA ((f16)9728)
#define IMPACT_HEIGHT_NUM ((s16)30720)
#define IMPACT_NEAR_DEPTH ((f16)32)
#define IMPACT_FAR_DEPTH ((f16)512)
#define MAX_VISIBLE_SPRITES 8

/* Frames an impact stays on screen. One frame is 50ms at 20fps, which is too
   brief to register, so the hit is held in world space and redrawn. */
#define IMPACT_FRAMES 3
#define IMPACT_FRAME_WALL 0
#define IMPACT_FRAME_ENEMY 1

/* Lifts a wall impact off the surface it hit so it depth tests in front of
   that wall rather than tying with it. */
#define IMPACT_WALL_LIFT ((f16)24)

/* Held in world coordinates rather than as a screen column, so it stays on
   the thing that was hit while the player turns during those frames. */
typedef struct impact_t
{
	f16 x, y;
	u8 framesLeft;
	u8 frame;
} impact_t;

static impact_t impact = {0};

static void setImpact(const f16 x, const f16 y, const u8 frame)
{
	impact.x = x;
	impact.y = y;
	impact.frame = frame;
	impact.framesLeft = IMPACT_FRAMES;
}

/* 60 degree FOV, 60 rays, 4 pixels each. Held as sincos_tab index offsets
   rather than as angles, so a ray direction is one add onto the player's table
   index instead of a trigidx() conversion per ray. Spans 171 of the 1024
   entries, which is the same 60.1 degrees as the angle table it replaces. */
static const s16 rayIdxOffset[60] =
{
	-86, -83, -80, -78, -74, -72, -69, -66, -63, -60,
	-57, -55, -51, -48, -46, -43, -40, -37, -34, -31,
	-29, -25, -23, -20, -16, -14, -11, -8, -5, -2,
	1, 3, 7, 9, 12, 15, 18, 21, 24, 27,
	29, 33, 35, 38, 41, 44, 47, 50, 53, 56,
	59, 61, 64, 67, 70, 73, 76, 78, 82, 85
};

static void resolvePlayerShot(const spritehit_t* spriteHits, const u16 spritesHit,
	const f16* f_wallDepth, const s16 baseIdx)
{
	u16 i;
	u8 aimSpan;
	s16 aimX;
	f16 f_targetDepth;
	u8 targetId = SPRITE_NO_ENEMY;

	if(!player.weaponState.shotPending)
		return;

	aimSpan = player.weaponState.shotSpan;
	aimX = (aimSpan << 2) + 2;
	f_targetDepth = f_wallDepth[aimSpan];

	for(i = 0; i < spritesHit; i++)
	{
		const spritehit_t* hit = &spriteHits[i];
		s16 width;
		s16 left;

		if(hit->enemyId == SPRITE_NO_ENEMY || hit->spriteHeight <= 0)
			continue;

		width = hit->spriteHeight;
		if(width < 1)
			width = 1;

		left = (hit->spanX << 2) + 2 - (width >> 1);

		if(aimX < left || aimX >= left + width)
			continue;

		if(hit->f_spriteDist >= f_wallDepth[hit->spanX] ||
			hit->f_spriteDist >= f_targetDepth)
			continue;

		targetId = hit->enemyId;
		f_targetDepth = hit->f_spriteDist;
	}

	player.weaponState.shotPending = FALSE;

	if(targetId != SPRITE_NO_ENEMY)
	{
		/* Read the position before damaging, so a killing blow still marks
		   where the enemy was standing. */
		const enemy_t* enemy = getEnemy(targetId);

		if(enemy)
			setImpact(enemy->x, enemy->y, IMPACT_FRAME_ENEMY);

		damageEnemy(targetId, player.currentWeapon->damage);
		return;
	}

	if(f_wallDepth[aimSpan] != FP_MAX)
	{
		/* The ray this shot travelled down, rebuilt from the same table the
		   cast used, so the hit can be placed in the world. There is no
		   fisheye correction, so f_wallDepth is distance along that ray. */
		const s16 rayIdx = baseIdx + rayIdxOffset[aimSpan];
		const f16 f_dx = sincos_tab[(rayIdx + TRIG_COS_OFFSET) & TRIG_TABLE_MASK];
		const f16 f_dy = sincos_tab[rayIdx & TRIG_TABLE_MASK];

		f16 f_impactDepth = f_wallDepth[aimSpan] - IMPACT_WALL_LIFT;

		if(f_impactDepth < IMPACT_NEAR_DEPTH)
			f_impactDepth = IMPACT_NEAR_DEPTH;

		setImpact(player.pos.x + fpmul(f_dx, f_impactDepth),
			player.pos.y + fpmul(f_dy, f_impactDepth),
			IMPACT_FRAME_WALL);
	}
}

/* Drawn after the enemies so a hit marker sits on top of the target rather
   than being painted over by it. */
static void drawImpact(const f16* f_wallDepth, const f16 f_viewCos, const f16 f_viewSin)
{
	spritehit_t hit;

	if(impact.framesLeft == 0)
		return;

	impact.framesLeft--;

	if(!projectSprite(impact.x, impact.y, &hit, f_viewCos, f_viewSin))
		return;

	if(hit.f_spriteDist >= f_wallDepth[hit.spanX])
		return;

	/* Distant impacts would otherwise shrink to a couple of pixels, so hold a
	   floor on the apparent size the way the original wall impact did. */
	if(hit.f_spriteDist > IMPACT_FAR_DEPTH)
		hit.spriteHeight = IMPACT_HEIGHT_NUM / IMPACT_FAR_DEPTH;

	hit.spriteId = (u8)((SPRITE_SLOT_PARTICLES << 3) | impact.frame);
	hit.mirrored = FALSE;
	hit.enemyId = SPRITE_NO_ENEMY;

	drawProjectedSprite(&hit);
}

/* rayDelta() of every sincos_tab entry. Ray directions are always table
   entries, so the divide is hoisted out of the frame entirely. */
static f16 recipTab[TRIG_TABLE_LEN];
static u8 recipReady = FALSE;

static f16 rayDelta(const f16 f_dir)
{
	u16 dir = f_dir < 0 ? -f_dir : f_dir;
	f16 delta;

	if(dir <= 2)
		return FP_MAX;

	/* Match 65536 / dir using a 16-bit dividend. */
	delta = ((u16)0xffff) / dir;

	if((dir & (dir - 1)) == 0)
		delta++;

	return delta;
}

void draw()
{
	u16 i;
	u16 spritesMarked = 0;
	u16 spritesHit = 0;
	markedsprite_t markedSprites[MAX_VISIBLE_SPRITES];
	spritehit_t spriteHits[MAX_VISIBLE_SPRITES];
	f16 f_wallDepth[60];
	const f16 f_viewCos = fpcos(player.pos.angle);
	const f16 f_viewSin = fpsin(player.pos.angle);
	const s16 baseIdx = trigidx(player.pos.angle);

	if(!recipReady)
	{
		u16 k;

		for(k = 0; k < TRIG_TABLE_LEN; k++)
			recipTab[k] = rayDelta(sincos_tab[k]);

		recipReady = TRUE;
	}

	for(i = 0; i < 60; i++)
	{
		wallhit_t wallhits[3];
	
		s16 mapx = fp2int(player.pos.x);
		s16 mapy = fp2int(player.pos.y);

		s16 stepx, stepy;
		f16 f_sidedx, f_sidedy;
		
		u16 side;
		
		u16 solid, hits;
	
		const s16 rayIdx = baseIdx + rayIdxOffset[i];
		const u16 cosIdx = (u16)((rayIdx + TRIG_COS_OFFSET) & TRIG_TABLE_MASK);
		const u16 sinIdx = (u16)(rayIdx & TRIG_TABLE_MASK);

		const f16 f_dx = sincos_tab[cosIdx];
		const f16 f_dy = sincos_tab[sinIdx];

		const f16 f_deltax = recipTab[cosIdx];
		const f16 f_deltay = recipTab[sinIdx];

		f_wallDepth[i] = FP_MAX;


		if(f_dx < 0)
		{
			stepx = -1;
			f_sidedx = fpmul(player.pos.x - int2fp(mapx), f_deltax);
		}
		else
		{
			stepx = 1;
			f_sidedx = fpmul(int2fp(mapx + 1) - player.pos.x, f_deltax);
		}

		if(f_dy < 0)
		{
			stepy = -1;
			f_sidedy = fpmul(player.pos.y - int2fp(mapy), f_deltay);
		}
		else
		{
			stepy = 1;
			f_sidedy = fpmul(int2fp(mapy + 1) - player.pos.y, f_deltay);
		}
		
		solid = 0;
		hits = 0;
		
		do
		{
			u16 hit;
			u16 hitcell;
			f16 f_dist;
			f16 f_wallx;
			
			do
			{
				if(f_sidedx < f_sidedy)
				{
					f_sidedx = f_sidedx + f_deltax;
					mapx += stepx;
					side = 0;
				}
				else
				{
					f_sidedy = f_sidedy + f_deltay;
					mapy += stepy;
					side = 1;
				}

				hitcell = mapCell(mapx, mapy);
				hit = isWall(hitcell);

				if(hit == 0)
				{
					if(isSprite(hitcell) && !isMarked(hitcell) &&
						spritesMarked < MAX_VISIBLE_SPRITES)
					{
						markSprite(mapx, mapy);
						markedSprites[spritesMarked].x = mapx;
						markedSprites[spritesMarked].y = mapy;
						spritesMarked++;
						
						if(isEnemy(hitcell))
						{
							enemy_t* enemy = getEnemy(GET_CELL_ID(hitcell));

							if(enemy && spritesHit < MAX_VISIBLE_SPRITES &&
								projectSprite(enemy->x, enemy->y, &spriteHits[spritesHit], f_viewCos, f_viewSin))
							{
								spriteHits[spritesHit].enemyId = (u8)GET_CELL_ID(hitcell);
								spriteHits[spritesHit].spriteId = ((enemy->enemyStats->spriteId << 3) | enemy->spriteFrame);
								spriteHits[spritesHit].mirrored = enemy->spriteMirrored &&
									(enemy->spriteFrame == ENEMY_FRAME_WALK_R1 ||
									 enemy->spriteFrame == ENEMY_FRAME_WALK_R2);
								spritesHit++;
							}
						}
						else
						{
							if(spritesHit < MAX_VISIBLE_SPRITES &&
								projectSprite(int2fp(mapx) + flt2fp(0.5f), int2fp(mapy) + flt2fp(0.5f), &spriteHits[spritesHit], f_viewCos, f_viewSin))
							{
								//TODO: Impliment sprite selection here. For now 0..3 are populated
								spriteHits[spritesHit].spriteId = 0;
								spriteHits[spritesHit].mirrored = FALSE;
								spriteHits[spritesHit].enemyId = SPRITE_NO_ENEMY;
								spritesHit++;
							}
						}
					}
				}
			} while(hit == 0);
			
			solid = isSolid(hitcell) || hits >= 2;
			
			wallhits[hits].cell = hitcell;
			wallhits[hits].side = side;

			/* The side distance was advanced past the boundary just crossed, so
			   subtracting one delta recovers the distance to it, without the
			   bit-serial 32 bit divide the old fpdiv needed. Near axis-parallel
			   rays keep the divide: their delta is large enough that the s16
			   accumulator can wrap inside the map, and the subtraction would
			   then return a near-zero distance and paint a full height column. */
			if(side == 0)
			{
				f_dist = (f_deltax < DDA_SAFE_DELTA)
					? (f16)(f_sidedx - f_deltax)
					: fpdiv((int2fp(mapx) - player.pos.x) + int2fp(((1-stepx)>>1)), f_dx);

				f_wallx = player.pos.y + fpmul(f_dist, f_dy);
			}
			else
			{
				f_dist = (f_deltay < DDA_SAFE_DELTA)
					? (f16)(f_sidedy - f_deltay)
					: fpdiv((int2fp(mapy) - player.pos.y) + int2fp(((1-stepy)>>1)), f_dy);

				f_wallx = player.pos.x + fpmul(f_dist, f_dx);
			}

			if(f_dist <= 0)
				f_dist = 1;

			wallhits[hits].wallHeight = WALL_HEIGHT_NUM / f_dist;
			wallhits[hits].f_wallDist = f_dist;
			wallhits[hits].f_wallX = f_wallx - int2fp(fp2int(f_wallx));
			
			hits++;
			
		} while(solid == 0);
		
		
		while(hits > 0)
		{
			hits--;

			if(drawWall(i << 2, &wallhits[hits]))
				f_wallDepth[i] = wallhits[hits].f_wallDist;
		}
	}
	
	resolvePlayerShot(spriteHits, spritesHit, f_wallDepth, baseIdx);

	while(spritesHit > 0)
	{
		spritesHit--;

		if(spriteHits[spritesHit].f_spriteDist < f_wallDepth[spriteHits[spritesHit].spanX])
			drawProjectedSprite(&spriteHits[spritesHit]);
	}

	drawImpact(f_wallDepth, f_viewCos, f_viewSin);
	
	while(spritesMarked > 0)
	{
		spritesMarked--;
		
		unmarkSprite(markedSprites[spritesMarked].x, markedSprites[spritesMarked].y);
	}

	//Draw player weapon, lowered by the switch animation and the firing recoil.
	drawSprite(player.currentWeapon->spanX,
		(u8)(player.currentWeapon->y + player.weaponState.switchOffset
			+ player.weaponState.recoilOffset),
		player.weaponState.weaponSpriteId);

	//Add rect around screen.
	bmDrawRect(0, 0, 240, 160, blackBm);

}
