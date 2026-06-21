#include <plib.h>
#include <wlib.h>

#include "psion3d.h"

typedef struct wallhit_t
{
	s16 wallHeight;
	f16 f_wallDist;
	f16 f_wallX;
	s8 cell;
	u8 side;
} wallhit_t;

typedef struct spritehit_t
{
	s16 spriteHeight;
	f16 f_spriteDist;
	u16 spanX;
} spritehit_t;

typedef struct markedsprite_t
{
	u8 x, y;
} markedsprite_t;

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

static u16 projectSprite(u16 x, u16 y, spritehit_t* hit)
{
    f16 f_rx = int2fp(x) - pos.x + flt2fp(0.5f);
    f16 f_ry = int2fp(y) - pos.y + flt2fp(0.5f);

    f16 f_depth =
        fpmul(f_rx, fpcos(pos.angle)) +
        fpmul(f_ry, fpsin(pos.angle));

    f16 f_side;
    s16 spanx;

    if(f_depth <= 0)
        return FALSE;

    f_side =
        -fpmul(f_rx, fpsin(pos.angle)) +
         fpmul(f_ry, fpcos(pos.angle));

    spanx = 30 + fp2int(
        fpmul(
            fpdiv(f_side, f_depth),
            int2fp(52)
        )
    );

    if(spanx < 0 || spanx >= 60)
        return FALSE;

	hit->spriteHeight = (s16)(((s32)120 << FP_BITS) / f_depth);
    hit->f_spriteDist = f_depth;
    hit->spanX = spanx;

    return TRUE;
}

static void brickPattern(P_RECT* rect, wallhit_t* hit, u16 mode)
{
	s16 qheight = (hit->wallHeight >> 2);
	s16 top = rect->tl.y;

	s16 wallx = hit->f_wallX; //FP -> 0..255

	gSetGC0(gc[BM_BLK]);

	rect->tl.y = 80;
	rect->br.y = 81;
	gClrRect(rect, mode);

	rect->tl.y -= qheight;
	rect->br.y -= qheight;
	gClrRect(rect, mode);

	rect->tl.y += (qheight << 1);
	rect->br.y += (qheight << 1);
	gClrRect(rect, mode);	

	if((wallx >= 64 && wallx < 72) || (wallx >= 192 && wallx < 200))
	{		
		rect->tl.y = top;
		rect->br.y = top + qheight;
		rect->br.x = rect->tl.x + 1;
		gClrRect(rect, mode);

		rect->tl.y = 80;
		rect->br.y = 80 + qheight;
		gClrRect(rect, mode);
	}
}

static void depthWall(P_RECT* rect, wallhit_t* hit)
{
	s16 depth = fp2int(hit->f_wallDist);

	gSetGC0(gc[BM_BLK]);

	if(depth >= 6)
	{
		gClrRect(rect, G_TRMODE_SET);
		return;
	}

	gFillPattern(rect, WS_BITMAP_GREY, G_TRMODE_REPL);
	
	gSetGC0(gc[BM_GRY]);

	if(depth >= 4 || hit->side)
	{
		gClrRect(rect, G_TRMODE_SET);
	}
	else
	{
		gClrRect(rect, G_TRMODE_CLR);
	}
}

static u16 drawWallX(P_RECT* rect, wallhit_t* hit)
{	
	depthWall(rect, hit);

	if(fp2int(hit->f_wallDist) < 6)
		brickPattern(rect, hit, G_TRMODE_SET);

	return TRUE;
}

static u16 drawWallA(P_RECT* rect, wallhit_t* hit)
{
	s16 wallx = hit->f_wallX; //FP 0..1 -> 0..255

	if(wallx > 24 && wallx < 232)
	{
		rect->br.y = rect->tl.y + (hit->wallHeight >> 2);
		depthWall(rect, hit);

		return FALSE;
	}
	
	depthWall(rect, hit);

	return TRUE;
}

static u16 drawWallD(P_RECT* rect, wallhit_t* hit)
{
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
		gSetGC0(gc[BM_BLK]);
		gClrRect(rect, G_TRMODE_SET);
	}
	else if(wallx < dleft || wallx > dright)
	{
		s16 height = (rect->br.y - rect->tl.y);

		if((wallx < (dleft - 2) && wallx >= (dleft - 5)) || (wallx > (dright + 2) && wallx <= (dright + 5)))
		{
			//Grey door face.
			gSetGC0(gc[BM_GRY]);
			gClrRect(rect, G_TRMODE_SET);

			//Clear black behind
			gSetGC0(gc[BM_BLK]);
			
			rect->br.y = rect->tl.y + (height >> 2);
			gClrRect(rect, G_TRMODE_CLR);

			rect->br.y = rect->tl.y + height;
			rect->tl.y += (height >> 1);
			gClrRect(rect, G_TRMODE_CLR);
		}
		else
		{
			gSetGC0(gc[BM_BLK]);
			gClrRect(rect, G_TRMODE_CLR);
			
			gSetGC0(gc[BM_GRY]);
			gClrRect(rect, G_TRMODE_SET);
		}

		gSetGC0(gc[BM_BLK]);
		rect->tl.y = rect->br.y - (height >> 3);

		gFillPattern(rect, WS_BITMAP_GREY, G_TRMODE_REPL);
	}
	else
	{
		return FALSE;
	}

	return TRUE;
}

static u16 drawWallP(P_RECT* rect, wallhit_t* hit)
{
	gSetGC0(gc[BM_BLK]);
	gClrRect(rect, G_TRMODE_SET);

	if(fp2int(hit->f_wallDist) < 6)
		brickPattern(rect, hit, G_TRMODE_CLR);

	return TRUE;
}

static u16 drawWallB(P_RECT* rect, wallhit_t* hit)
{
	s16 top = rect->tl.y;
	s16 bottom = rect->br.y;

	gSetGC0(gc[BM_BLK]);

	rect->br.y = top + (hit->wallHeight >> 3);

	depthWall(rect, hit);

	rect->br.y = bottom;
	rect->tl.y = bottom - (hit->wallHeight >> 3);
	depthWall(rect, hit);

	if((hit->f_wallX >> 3) & 1)
	{
		rect->tl.y = top + (hit->wallHeight >> 3);
		rect->br.y = bottom - (hit->wallHeight >> 3);

		gSetGC0(gc[BM_BLK]);
		gClrRect(rect, G_TRMODE_SET);

		return TRUE;
	}

	return FALSE;
}

static u16 drawWallW(P_RECT* rect, wallhit_t* hit)
{
	s16 top = rect->tl.y;
	s16 bottom = rect->br.y;


	rect->tl.y += (hit->wallHeight >> 2);
	rect->br.y -= (hit->wallHeight >> 2);

	gSetGC0(gc[BM_GRY]);
	gClrRect(rect, G_TRMODE_SET);

	gSetGC0(gc[BM_BLK]);
	gDrawLine(rect->tl.x, rect->tl.y, rect->tl.x + 4, rect->tl.y);
	gDrawLine(rect->tl.x, rect->br.y, rect->tl.x + 4, rect->br.y);
	
	rect->tl.y = top;
	rect->br.y = top + (hit->wallHeight >> 2);
	depthWall(rect, hit);

	rect->tl.y = bottom - (hit->wallHeight >> 2);
	rect->br.y = bottom;
	
	depthWall(rect, hit);
		
	return FALSE;
}

static u16 drawWallV(P_RECT* rect, wallhit_t* hit)
{
	rect->tl.y = 80;
	rect->br.y = 81;

	gSetGC0(gc[BM_BLK]);
	gClrRect(rect, G_TRMODE_SET);

	return FALSE;
}

static u16 drawWall(u16 x, wallhit_t* hit)
{
	u16 updateZ = TRUE;
	u16 height =  hit->wallHeight;
	P_RECT rect;
	
	rect.tl.x = x;
	rect.tl.y = 80 - (height >> 1);
	rect.br.x = x + 4;
	rect.br.y = 80 + (height >> 1);
	
	switch(hit->cell)
	{
		case 'W':
			hit->side = 0;
			break;
		case 'S':
			rect.tl.y += 2;
			rect.br.y -= 2;
			hit->cell = 'X';
			hit->side = 1 - hit->side;
			break;
	}

	switch(hit->cell)
	{
		case 'X':
			updateZ = drawWallX(&rect, hit);
			break;
		case 'A':
			updateZ = drawWallA(&rect, hit);
			break;
		case 'D':
			updateZ = drawWallD(&rect, hit);
			break;
		case 'P':
			updateZ = drawWallP(&rect, hit);
			break;
		case 'B':
			updateZ = drawWallB(&rect, hit);
			break;
		case 'W':
			updateZ = drawWallW(&rect, hit);
			break;
		case 'V':
			updateZ = drawWallV(&rect, hit);
			break;
	}

	return updateZ;
}

static void drawSprite(spritehit_t* spriteHit)
{
	P_RECT rect;

	rect.tl.x = ((spriteHit->spanX * 4) + 2) - (spriteHit->spriteHeight >> 2);
	rect.tl.y = 80 + (spriteHit->spriteHeight >> 2);
	rect.br.x = rect.tl.x + (spriteHit->spriteHeight >> 1);
	rect.br.y = rect.tl.y + (spriteHit->spriteHeight >> 2);

	gSetGC0(gc[BM_BLK]);
	gClrRect(&rect, G_TRMODE_SET);
}

void draw()
{
	u16 i;
	u16 spritesMarked = 0;
	u16 spritesHit = 0;
	markedsprite_t markedSprites[8];
	spritehit_t spriteHits[8];
	f16 f_wallDepth[60];

	for(i = 0; i < 60; i++)
	{
		wallhit_t wallhits[3];
	
		s16 mapx = fp2int(pos.x);
		s16 mapy = fp2int(pos.y);

		s16 stepx, stepy;
		f16 f_deltax, f_deltay;
		f16 f_sidedx, f_sidedy;
		
		u16 side;
		
		u16 solid, hits;
	
		f16 f_ra = pos.angle + rayAngleOffset[i];

		f16 f_dx = fpcos(f_ra);
		f16 f_dy = fpsin(f_ra);

		f_wallDepth[i] = int2fp(127);

		if(f_dx < 0)
		{
			stepx = -1;
			f_deltax = fpdiv(int2fp(-1), f_dx);
			f_sidedx = fpmul(pos.x - int2fp(mapx), f_deltax);
		}
		else
		{
			stepx = 1;
			f_deltax = fpdiv(int2fp(1), f_dx);
			f_sidedx = fpmul(int2fp(mapx + 1) - pos.x, f_deltax);
		}

		if(f_dy < 0)
		{
			stepy = -1;
			f_deltay = fpdiv(int2fp(-1), f_dy);
			f_sidedy = fpmul(pos.y - int2fp(mapy), f_deltay);
		}
		else
		{
			stepy = 1;
			f_deltay = fpdiv(int2fp(1), f_dy);
			f_sidedy = fpmul(int2fp(mapy + 1) - pos.y, f_deltay);
		}
		

		solid = 0;
		hits = 0;
		
		do
		{
			u16 hit = 0;
			s8 hitcell;
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
					s8 xcell = mapCell(xmap, mapy);
					s8 ycell = mapCell(mapx, ymap);

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
					if(isSprite(hitcell))
					{
						markSprite(mapx, mapy);
						markedSprites[spritesMarked].x = mapx;
						markedSprites[spritesMarked].y = mapy;
						spritesMarked++;
						
						if(projectSprite(mapx, mapy, &spriteHits[spritesHit]))
							spritesHit++;
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

	gSetGC0(gc[BM_BLK]);
	gDrawBox(&gameWinRect);
}
