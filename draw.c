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
#define IMPACT_HEIGHT_NUM ((s16)30720)
#define IMPACT_NEAR_DEPTH ((f16)32)
#define IMPACT_FAR_DEPTH ((f16)512)
#define MAX_VISIBLE_SPRITES 8

static u16 resolvePlayerShot(const spritehit_t* spriteHits, const u16 spritesHit,
	const f16* f_wallDepth, spritehit_t* wallImpact)
{
	u16 i;
	u8 aimSpan;
	s16 aimX;
	f16 f_targetDepth;
	u8 targetId = SPRITE_NO_ENEMY;

	if(!player.weaponState.shotPending)
		return FALSE;

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

	if(targetId != SPRITE_NO_ENEMY)
	{
		damageEnemy(targetId, player.currentWeapon->damage);
		player.weaponState.shotPending = FALSE;
		return FALSE;
	}

	if(f_wallDepth[aimSpan] != FP_MAX)
	{
		f16 f_impactDepth = f_wallDepth[aimSpan];

		if(f_impactDepth > IMPACT_FAR_DEPTH)
			f_impactDepth = IMPACT_FAR_DEPTH;

		if(f_impactDepth < IMPACT_NEAR_DEPTH)
			f_impactDepth = IMPACT_NEAR_DEPTH;

		wallImpact->spriteHeight = IMPACT_HEIGHT_NUM / f_impactDepth;
		wallImpact->f_spriteDist = f_wallDepth[aimSpan];
		wallImpact->spanX = aimSpan;
		wallImpact->spriteId = SPRITE_SLOT_PARTICLES << 3;
		wallImpact->mirrored = FALSE;
		wallImpact->enemyId = SPRITE_NO_ENEMY;
		player.weaponState.shotPending = FALSE;
		return TRUE;
	}

	player.weaponState.shotPending = FALSE;
	return FALSE;
}

/* 60 degree FOV, 60 rays, 4 pixels each */
static const f16 rayAngleOffset[60] =
{
	-134, -130, -125, -121, -116, -112, -107, -103, -98, -94,
	-89, -85, -80, -75, -71, -66, -62, -57, -53, -48,
	-44, -39, -35, -30, -25, -21, -16, -12, -7, -3,
	2, 6, 11, 15, 20, 24, 29, 34, 38, 43,
	47, 52, 56, 61, 65, 70, 74, 79, 84, 88,
	93, 97, 102, 106, 111, 115, 120, 124, 129, 134
};

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
	u16 drawWallImpact;
	markedsprite_t markedSprites[MAX_VISIBLE_SPRITES];
	spritehit_t spriteHits[MAX_VISIBLE_SPRITES];
	spritehit_t wallImpact;
	f16 f_wallDepth[60];
	const f16 f_viewCos = fpcos(player.pos.angle);
	const f16 f_viewSin = fpsin(player.pos.angle);

	for(i = 0; i < 60; i++)
	{
		wallhit_t wallhits[3];
	
		s16 mapx = fp2int(player.pos.x);
		s16 mapy = fp2int(player.pos.y);

		s16 stepx, stepy;
		f16 f_sidedx, f_sidedy;
		
		u16 side;
		
		u16 solid, hits;
	
		const f16 f_ra = player.pos.angle + rayAngleOffset[i];

		const f16 f_dx = fpcos(f_ra);
		const f16 f_dy = fpsin(f_ra);

		const f16 f_deltax = rayDelta(f_dx);
		const f16 f_deltay = rayDelta(f_dy);

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

					hitcell = mapCell(mapx, mapy);
					hit = isWall(hitcell);
				}
				else
				{
					f_sidedy = f_sidedy + f_deltay;
					mapy += stepy;
					side = 1;

					hitcell = mapCell(mapx, mapy);
					hit = isWall(hitcell);
				}
				
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

			if(side == 0)
			{
				f_dist = fpdiv((int2fp(mapx) - player.pos.x) + int2fp(((1-stepx)>>1)), f_dx);
				f_wallx = player.pos.y + fpmul(f_dist, f_dy);
			}
			else
			{
				f_dist = fpdiv((int2fp(mapy) - player.pos.y) + int2fp(((1-stepy)>>1)), f_dy);
				f_wallx = player.pos.x + fpmul(f_dist, f_dx);
			}

			if(f_dist == 0)
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
	
	drawWallImpact = resolvePlayerShot(spriteHits, spritesHit, f_wallDepth, &wallImpact);

	if(drawWallImpact)
		drawProjectedSprite(&wallImpact);

	while(spritesHit > 0)
	{
		spritesHit--;
		
		if(spriteHits[spritesHit].f_spriteDist < f_wallDepth[spriteHits[spritesHit].spanX])
			drawProjectedSprite(&spriteHits[spritesHit]);
	}
	
	while(spritesMarked > 0)
	{
		spritesMarked--;
		
		unmarkSprite(markedSprites[spritesMarked].x, markedSprites[spritesMarked].y);
	}

	//Draw player weapon.
	drawSprite(player.currentWeapon->spanX, player.currentWeapon->y, player.weaponState.weaponSpriteId);

	//Add rect around screen.
	bmDrawRect(0, 0, 240, 160, blackBm);

}
