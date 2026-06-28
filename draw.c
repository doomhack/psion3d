#include <plib.h>
#include <wlib.h>

#include "psion3d.h"
#include "bitmap.h"
#include "sprite.h"
#include "game_map.h"
#include "enemy.h"

typedef struct wallhit_t
{
	s16 wallHeight;
	f16 f_wallDist;
	f16 f_wallX;
	u16 cell;
	u8 side;
} wallhit_t;

typedef struct markedsprite_t
{
	u8 x, y;
} markedsprite_t;

#define MODE_FILL 0
#define MODE_CLEAR 1
#define MODE_INVERT 2

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

static void brickPattern(s16 x, s16 y, s16 w, s16 h, const wallhit_t* hit, const u16 mode)
{
	//Draw a brick pattern on a wall.

	s16 qheight = (hit->wallHeight >> 2);

	s16 wallx = hit->f_wallX; //FP -> 0..255

	if(mode == MODE_FILL)
	{
		bmFillRect4(x, 80, 1, blackBm);
		bmFillRect4(x, 80 - qheight, 1, blackBm);
		bmFillRect4(x, 80 + qheight, 1, blackBm);
	}
	else if(mode == MODE_CLEAR)
	{
		bmClearRect4(x, 80, 1, blackBm);
		bmClearRect4(x, 80 - qheight, 1, blackBm);
		bmClearRect4(x, 80 + qheight, 1, blackBm);
	}

	if((wallx >= 64 && wallx < 72) || (wallx >= 192 && wallx < 200))
	{
		if(mode == MODE_FILL)
		{
			bmFillRect(x, y, 1, qheight, blackBm);
			bmFillRect(x, 80, 1, qheight, blackBm);
		}
		else if(mode == MODE_CLEAR)
		{
			bmClearRect(x, y, 1, qheight, blackBm);
			bmClearRect(x, 80, 1, qheight, blackBm);
		}
	}
}

static void depthWall(s16 x, s16 y, s16 w, s16 h, const wallhit_t* hit)
{
	//Draw a wall which is darker further away and contrast north/south from east/west walls.

	s16 depth = fp2int(hit->f_wallDist);

	if(depth >= 6)
	{
		bmFillRect4(x, y, h, blackBm);
		return;
	}

	bmFillPattern4(x, y, h, blackBm);
	

	if(depth >= 4 || hit->side)
	{
		bmFillRect4(x, y, h, greyBm);
	}
	else
	{
		bmClearRect4(x, y, h, greyBm);
	}
}

static u16 drawWallX(s16 x, s16 y, s16 w, s16 h, const wallhit_t* hit)
{
	//Brick wall. Distance depth effect.
	
	depthWall(x, y, w, h, hit);

	if(fp2int(hit->f_wallDist) < 6)
		brickPattern(x, y, w, h, hit, MODE_FILL);

	return TRUE;
}

static u16 drawWallA(s16 x, s16 y, s16 w, s16 h, const wallhit_t* hit)
{
	//Archway. Distance depth effect.

	s16 wallx = hit->f_wallX; //FP 0..1 -> 0..255

	if(wallx > 24 && wallx < 232)
	{
		depthWall(x, y, w, hit->wallHeight >> 2, hit);

		return FALSE;
	}
	
	depthWall(x, y, w, h, hit);

	return TRUE;
}

static u16 drawWallD(s16 x, s16 y, s16 w, s16 h, const wallhit_t* hit)
{
	//Unlocked Door which opens when near.
	//Horizontal opening pair of doors.

	s16 doorgap, dleft, dright;

	s16 dist = hit->f_wallDist >> 4;
	s16 wallx = (hit->f_wallX >> 4); //FP 0..1 -> 0..16

	if(dist > 16)
		doorgap = 0;
	else
		doorgap = 16 - dist;

	dleft = 8 - (doorgap >> 1);
	dright = 8 + (doorgap >> 1);

	if(wallx == dleft || wallx == dright)
	{
		//Black edge of door.

		bmFillRect4(x, y, h, blackBm);
	}
	else if(wallx < dleft || wallx > dright)
	{
		if((wallx < (dleft - 2) && wallx >= (dleft - 5)) || (wallx > (dright + 2) && wallx <= (dright + 5)))
		{
			//Grey door face.
			bmFillRect4(x, y, h, greyBm);

			//Clear black behind
			bmClearRect4(x, y, (h >> 2), blackBm); //Top qtr.
			bmClearRect4(x, y + (h >> 1), (h >> 1), blackBm); //Bottom half.
		}
		else
		{
			bmClearRect4(x, y, h, blackBm);
			bmFillRect4(x, y, h, greyBm);
		}


		//Kickplate.
		bmFillPattern4(x, y + h - (h >> 3), h >> 3, blackBm);
	}
	else
	{
		return FALSE;
	}

	return TRUE;
}

static u16 drawWallP(s16 x, s16 y, s16 w, s16 h, const wallhit_t* hit)
{
	//Black wall with brick effect.

	bmFillRect4(x, y, h, blackBm);

	if(fp2int(hit->f_wallDist) < 6)
		brickPattern(x, y, w, h, hit, MODE_CLEAR);

	return TRUE;
}

static u16 drawWallB(s16 x, s16 y, s16 w, s16 h, const wallhit_t* hit)
{
	//Opening with bars.

	s16 top = y;
	s16 bottom = y + h;
	s16 capHeight = hit->wallHeight >> 3;

	depthWall(x, top, w, capHeight, hit);
	depthWall(x, bottom - capHeight, w, capHeight, hit);

	if((hit->f_wallX >> 3) & 1)
	{
		bmFillRect4(x, y + (h >> 3), h - (h >> 2), blackBm);

		return TRUE;
	}

	return FALSE;
}

static u16 drawWallW(s16 x, s16 y, s16 w, s16 h, const wallhit_t* hit)
{
	//Window opening with grey glass effect.

	s16 top = y;
	s16 bottom = y + h;
	s16 capHeight = hit->wallHeight >> 2;

	bmFillRect4(x, y + (h >> 2), h - (h >> 1), greyBm);
	
	depthWall(x, top, w, capHeight, hit);
	depthWall(x, bottom - capHeight, w, capHeight, hit);
		
	return FALSE;
}

static u16 drawWallV(s16 x, s16 y, s16 w, s16 h, const wallhit_t* hit)
{
	//The Void. Horizon line.

	bmFillRect4(x, 80, 1, blackBm);

	return FALSE;
}

static u16 drawWall(u16 x, wallhit_t* hit)
{
	u16 updateZ = TRUE;
	s16 y;
	s16 w = 4;
	s16 h = hit->wallHeight;
	u16 wall_type = GET_CELL_TYPE_ID(hit->cell);

	y = 80 - (h >> 1);
	

	switch(wall_type)
	{
		case WALL_TYPE_WINDOW:
			hit->side = 0;
			break;
		case WALL_TYPE_SECRET: 	//Secret wall. Can walk through. North-south shaing flipped and inset slightly.
			y += 2;
			h -= 4;
			hit->side = 1 - hit->side;
			break;
	}

	switch(wall_type)
	{
		case WALL_TYPE_BRICK:
		case WALL_TYPE_SECRET:
			updateZ = drawWallX(x, y, w, h, hit);
			break;
		case WALL_TYPE_ARCH:
			updateZ = drawWallA(x, y, w, h, hit);
			break;
		case WALL_TYPE_UNLOCKED_DOOR:
			updateZ = drawWallD(x, y, w, h, hit);
			break;
		case WALL_TYPE_DARK:
			updateZ = drawWallP(x, y, w, h, hit);
			break;
		case WALL_TYPE_BARS:
			updateZ = drawWallB(x, y, w, h, hit);
			break;
		case WALL_TYPE_WINDOW:
			updateZ = drawWallW(x, y, w, h, hit);
			break;
		case WALL_TYPE_VOID:
			updateZ = drawWallV(x, y, w, h, hit);
			break;
	}

	return updateZ;
}

void draw()
{
	u16 i;
	u16 spritesMarked = 0;
	u16 spritesHit = 0;
	markedsprite_t markedSprites[8];
	spritehit_t spriteHits[8];
	f16 f_wallDepth[60];
	const f16 f_viewCos = fpcos(pos.angle);
	const f16 f_viewSin = fpsin(pos.angle);

	for(i = 0; i < 60; i++)
	{
		wallhit_t wallhits[3];
	
		s16 mapx = fp2int(pos.x);
		s16 mapy = fp2int(pos.y);

		s16 stepx, stepy;
		f16 f_sidedx, f_sidedy;
		
		u16 side;
		
		u16 solid, hits;
	
		const f16 f_ra = pos.angle + rayAngleOffset[i];

		const f16 f_dx = fpcos(f_ra);
		const f16 f_dy = fpsin(f_ra);

		const f16 f_deltax = rayDelta(f_dx);
		const f16 f_deltay = rayDelta(f_dy);

		f_wallDepth[i] = FP_MAX;


		if(f_dx < 0)
		{
			stepx = -1;
			f_sidedx = fpmul(pos.x - int2fp(mapx), f_deltax);
		}
		else
		{
			stepx = 1;
			f_sidedx = fpmul(int2fp(mapx + 1) - pos.x, f_deltax);
		}

		if(f_dy < 0)
		{
			stepy = -1;
			f_sidedy = fpmul(pos.y - int2fp(mapy), f_deltay);
		}
		else
		{
			stepy = 1;
			f_sidedy = fpmul(int2fp(mapy + 1) - pos.y, f_deltay);
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
				else if(f_sidedy < f_sidedx)
				{
					f_sidedy = f_sidedy + f_deltay;
					mapy += stepy;
					side = 1;

					hitcell = mapCell(mapx, mapy);
					hit = isWall(hitcell);
				}
				else
				{
					s16 xmap = mapx + stepx;
					s16 ymap = mapy + stepy;
					u16 xcell = mapCell(xmap, mapy);
					u16 ycell = mapCell(mapx, ymap);

					if(isWall(xcell))
					{
						f_sidedx = f_sidedx + f_deltax;
						mapx = xmap;
						side = 0;
						hitcell = xcell;
						hit = TRUE;
					}
					else if(isWall(ycell))
					{
						f_sidedy = f_sidedy + f_deltay;
						mapy = ymap;
						side = 1;
						hitcell = ycell;
						hit = TRUE;
					}
					else
					{
						f_sidedx = f_sidedx + f_deltax;
						f_sidedy = f_sidedy + f_deltay;
						mapx = xmap;
						mapy = ymap;
						side = 0;

						hitcell = mapCell(mapx, mapy);
						hit = isWall(hitcell);
					}
				}
				
				if(hit == 0)
				{
					if(isSprite(hitcell) && !isMarked(hitcell))
					{
						markSprite(mapx, mapy);
						markedSprites[spritesMarked].x = mapx;
						markedSprites[spritesMarked].y = mapy;
						spritesMarked++;
						
						if(isEnemy(hitcell))
						{
							enemy_t* enemy = getEnemy(GET_CELL_ID(hitcell));

							if(enemy && projectSprite(enemy->x, enemy->y, &spriteHits[spritesHit], f_viewCos, f_viewSin))
							{
								spriteHits[spritesHit].spriteId = ((enemy->spriteId << 3) | enemy->spriteFrame);
								spritesHit++;
							}
						}
						else
						{
							if(projectSprite(int2fp(mapx) + flt2fp(0.5f), int2fp(mapy) + flt2fp(0.5f), &spriteHits[spritesHit], f_viewCos, f_viewSin))
							{
								//TODO: Impliment sprite selection here. For now 0..3 are populated
								spriteHits[spritesHit].spriteId = 0;
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
				f_dist = fpdiv((int2fp(mapx) - pos.x) + int2fp(((1-stepx)>>1)), f_dx);
				f_wallx = pos.y + fpmul(f_dist, f_dy);
			}
			else
			{
				f_dist = fpdiv((int2fp(mapy) - pos.y) + int2fp(((1-stepy)>>1)), f_dy);
				f_wallx = pos.x + fpmul(f_dist, f_dx);
			}

			wallhits[hits].wallHeight = (s16)(((s32)120 << FP_BITS) / f_dist);
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
	
	while(spritesHit > 0)
	{
		spritesHit--;
		
		if(spriteHits[spritesHit].f_spriteDist < f_wallDepth[spriteHits[spritesHit].spanX])
			drawSprite(&spriteHits[spritesHit]);
	}
	
	while(spritesMarked > 0)
	{
		spritesMarked--;
		
		unmarkSprite(markedSprites[spritesMarked].x, markedSprites[spritesMarked].y);
	}

	//Add rect around screen.
	bmDrawRect(0, 0, 240, 160, blackBm);

}
