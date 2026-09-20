#include "psion3d.h"
#include "bitmap.h"
#include "sprite.h"
#include "sprslot.h"
#include "game_map.h"
#include "enemy.h"
#include "decor.h"
#include "level.h"
#include "walls.h"
#include "draw.h"

typedef struct markedsprite_t
{
	u8 x, y;
} markedsprite_t;

#define WALL_HEIGHT_NUM ((s16)30720)

/* Side distance for an axis the ray runs exactly parallel to, and so never
   crosses. Bigger than any real side distance: those are u16 and bounded by
   the hit distance plus one delta, at most 23168 + 32767 over this map. */
#define DDA_NEVER ((u16)0xffff)
#define IMPACT_HEIGHT_NUM ((s16)30720)
#define IMPACT_NEAR_DEPTH ((f16)32)
#define IMPACT_FAR_DEPTH ((f16)512)
#define MAX_VISIBLE_SPRITES 8

/* Frames an impact stays on screen. One frame is 50ms at 20fps, which is too
   brief to register, so the hit is held in world space and redrawn. */
#define IMPACT_FRAMES 3
#define IMPACT_FRAME_WALL 0
#define IMPACT_FRAME_ENEMY 1

//Pixels of screen edge painted black while the player's hurt flash is up.
#define HIT_FLASH_THICKNESS 8

/* Lifts a wall impact off the surface it hit so it depth tests in front of
   that wall rather than tying with it. */
#define IMPACT_WALL_LIFT ((f16)24)

/* Vertical scatter of a hit, in sixty-fourths of the target's height. The
   floor keeps even the most accurate weapon from stamping every round on the
   same row; the ceiling keeps hits on the torso, since row 80 is eye level. */
#define IMPACT_VSPREAD_MIN 5
#define IMPACT_VSPREAD_MAX 12

/* Held in world coordinates rather than as a screen column, so it stays on
   the thing that was hit while the player turns during those frames. The
   offsets are fractions of the target's height rather than pixels, so the
   mark keeps its place on the target as its apparent size changes. */
typedef struct impact_t
{
	f16 x, y;
	s8 fracX, fracY;
	u8 framesLeft;
	u8 frame;
} impact_t;

static impact_t impact = {0};
static u16 impactRand = 0xb15f;

static u8 impactVSpread(const u8 accuracy)
{
	u16 band = IMPACT_VSPREAD_MIN + ((255 - accuracy) >> 4);

	if(band > IMPACT_VSPREAD_MAX)
		band = IMPACT_VSPREAD_MAX;

	return (u8)band;
}

/* A shot carries no vertical component - the ray is level with the player's
   eye - so how far up or down a round lands has to be invented. */
static s8 impactFracY(const u8 accuracy)
{
	u8 band = impactVSpread(accuracy);

	impactRand = (u16)(impactRand * 25173 + 13849);

	return (s8)((s16)(impactRand % ((band << 1) + 1)) - band);
}

static void setImpact(const f16 x, const f16 y, const u8 frame,
	const s8 fracX, const s8 fracY)
{
	impact.x = x;
	impact.y = y;
	impact.frame = frame;
	impact.fracX = fracX;
	impact.fracY = fracY;
	impact.framesLeft = IMPACT_FRAMES;
}

#define TRACER_SLOTS 4
#define TRACER_FRAMES 2

/* Sprites are centred on row 80, so that is the shooter's midpoint at any
   distance. The round converges near the bottom of the view rather than on
   the centre of it: the camera is the player, so an enemy dead ahead sits
   within a couple of pixels of the centre and a line drawn there would have
   no length at all. */
#define TRACER_START_Y 80
#define TRACER_END_X 120
#define TRACER_END_Y 152
#define TRACER_MISS_OFFSET 32

/* Centred on the column an accurate shot resolves against (span 30, x 122)
   and the sprite horizon. Four arms with a gap at the middle, XORed like the
   tracers so it reads against wall, sprite and open floor alike, and so no
   pixel is flipped twice where the arms would otherwise meet. */
#define CROSSHAIR_X 122
#define CROSSHAIR_Y 80
#define CROSSHAIR_GAP 2
#define CROSSHAIR_ARM 4

static void drawCrosshair(void)
{
	bmXorLine(CROSSHAIR_X - CROSSHAIR_GAP - CROSSHAIR_ARM, CROSSHAIR_Y,
		CROSSHAIR_X - CROSSHAIR_GAP, CROSSHAIR_Y, blackBm);
	bmXorLine(CROSSHAIR_X + CROSSHAIR_GAP, CROSSHAIR_Y,
		CROSSHAIR_X + CROSSHAIR_GAP + CROSSHAIR_ARM, CROSSHAIR_Y, blackBm);
	bmXorLine(CROSSHAIR_X, CROSSHAIR_Y - CROSSHAIR_GAP - CROSSHAIR_ARM,
		CROSSHAIR_X, CROSSHAIR_Y - CROSSHAIR_GAP, blackBm);
	bmXorLine(CROSSHAIR_X, CROSSHAIR_Y + CROSSHAIR_GAP,
		CROSSHAIR_X, CROSSHAIR_Y + CROSSHAIR_GAP + CROSSHAIR_ARM, blackBm);
}

typedef struct tracer_t
{
	u8 enemyId;
	u8 framesLeft;
	u8 aim;
} tracer_t;

static tracer_t tracers[TRACER_SLOTS] = {0};

void addEnemyTracer(u8 enemyId, u8 aim)
{
	u16 i;
	u16 slot = TRACER_SLOTS;

	for(i = 0; i < TRACER_SLOTS; i++)
	{
		/* A second shot from the same enemy refreshes its streak rather than
		   taking a second slot. */
		if(tracers[i].framesLeft > 0 && tracers[i].enemyId == enemyId)
		{
			tracers[i].aim = aim;
			tracers[i].framesLeft = TRACER_FRAMES;
			return;
		}

		if(tracers[i].framesLeft == 0 && slot == TRACER_SLOTS)
			slot = i;
	}

	if(slot < TRACER_SLOTS)
	{
		tracers[slot].enemyId = enemyId;
		tracers[slot].aim = aim;
		tracers[slot].framesLeft = TRACER_FRAMES;
	}
}

/* Drawn over the enemy sprites, since the round leaves the shooter and travels
   toward the player. The shooter's screen position is taken from the
   projection the sprite pass already did rather than projecting a second time. */
static void drawEnemyTracers(const spritehit_t* spriteHits, const u16 spritesHit,
	const f16* f_wallDepth)
{
	u16 i;

	for(i = 0; i < TRACER_SLOTS; i++)
	{
		u16 j;

		if(tracers[i].framesLeft == 0)
			continue;

		/* Aged whether or not it can be drawn, so a shooter that steps out of
		   view leaves no stale entry behind. */
		tracers[i].framesLeft--;

		for(j = 0; j < spritesHit; j++)
		{
			s16 endX;

			if(spriteHits[j].enemyId != tracers[i].enemyId)
				continue;

			if(spriteHits[j].f_spriteDist >= f_wallDepth[spriteHits[j].spanX])
				break;

			endX = TRACER_END_X;

			if(tracers[i].aim == TRACER_AIM_WIDE_L)
				endX -= TRACER_MISS_OFFSET;
			else if(tracers[i].aim == TRACER_AIM_WIDE_R)
				endX += TRACER_MISS_OFFSET;

			bmXorLine((s16)((spriteHits[j].spanX << 2) + 2), TRACER_START_Y,
				endX, TRACER_END_Y, blackBm);
			break;
		}
	}
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

/* Half the angular width of each column, in 1/4096ths of a radian: half the
   larger of the gaps to the rays either side of it. The offsets above are not
   evenly spaced - the gaps run 2, 3 and 4 entries - so a ray does not sit in
   the middle of its column, and a footprint that reaches a quarter of the way
   to each neighbour leaves a hole on the wide side that a feature can fall
   through. Reaching half the larger gap both ways closes it: the two columns
   either side of any gap meet at its middle at least. The overlap that
   creates on the narrow side means a feature narrower than a footprint can
   land in two neighbouring columns, and wall styles draw it a column wide
   rather than pick one. Generated from rayIdxOffset:
   round(max(off[i] - off[i-1], off[i+1] - off[i]) / 2 * 2pi / 1024 * 4096). */
static const u8 rayHalfWidth[60] =
{
	38, 38, 38, 50, 50, 38, 38, 38, 38, 38,
	38, 50, 50, 38, 38, 38, 38, 38, 38, 38,
	50, 50, 38, 50, 50, 38, 38, 38, 38, 38,
	38, 50, 50, 38, 38, 38, 38, 38, 38, 38,
	50, 50, 38, 38, 38, 38, 38, 38, 38, 38,
	38, 38, 38, 38, 38, 38, 38, 50, 50, 38
};

/* Which map cell a shot landed on. f_wallDepth holds the distance to the
   surface that claimed the column, so the cell itself starts a little further
   along the ray: step past that surface in sixteenths of a cell until a wall
   turns up. Three steps is enough to cross the boundary at any angle and far
   too short to tunnel into the cell behind. This runs only when a shot lands on
   a wall, so it costs nothing per frame. */
#define SHOT_PROBE_STEP ((f16)16)
#define SHOT_PROBE_STEPS 3

static void hitWallCell(const f16 f_depth, const f16 f_dx, const f16 f_dy)
{
	f16 f_probe = f_depth;
	u16 i;

	for(i = 0; i < SHOT_PROBE_STEPS; i++)
	{
		s16 x, y;
		u16 cell;

		f_probe += SHOT_PROBE_STEP;

		x = fp2int(player.pos.x + fpmul(f_dx, f_probe));
		y = fp2int(player.pos.y + fpmul(f_dy, f_probe));

		cell = mapCell((u16)x, (u16)y);

		if(!isWall(cell))
			continue;

		/* Switches are thrown with the use key, not shot: see tryUse() in
		   player.c. A round put through one does nothing. */
		if(mapCellType(cell) == WALL_TYPE_SHOOTABLE)
		{
			/* Open floor and nothing else. MAP_MASK_WALK on its own is exactly
			   what the enemy step test asks for, so a passage shot open is
			   usable by both sides. */
			updateCell((u16)x, (u16)y, MAP_MASK_WALK);
		}

		/* The first wall along the ray is the one that was shot, whatever its
		   type. Probing on past it would reach through it. */
		return;
	}
}

/* A sprite the round can land on: an enemy, or a decoration, which is solid
   to a shot as it is to a step. Pickups are neither - a round goes over them
   to whatever is behind. */
static u16 spriteStopsShot(const spritehit_t* hit)
{
	return hit->enemyId != SPRITE_NO_ENEMY ||
		(hit->spriteId >> 3) == SPRITE_SLOT_DECORATIONS;
}

static void resolvePlayerShot(const spritehit_t* spriteHits, const u16 spritesHit,
	const f16* f_wallDepth, const s16 baseIdx)
{
	u16 i;
	u8 aimSpan;
	s16 aimX;
	f16 f_targetDepth;
	const spritehit_t* target = 0;
	s16 targetWidth = 1;
	s16 targetCentreX = 0;

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

		if(!spriteStopsShot(hit) || hit->spriteHeight <= 0)
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

		target = hit;
		f_targetDepth = hit->f_spriteDist;
		targetWidth = width;
		targetCentreX = (hit->spanX << 2) + 2;
	}

	player.weaponState.shotPending = FALSE;

	if(target)
	{
		/* Where the round actually crossed the silhouette. The hit test above
		   already proved aimX lies within the target, so this is at most half
		   a width either side and the shift stays inside s16. */
		const s8 offsetFrac = (s8)(((aimX - targetCentreX) << 6) / targetWidth);
		const s8 fracY = impactFracY(player.currentWeapon->accuracy);

		if(target->enemyId != SPRITE_NO_ENEMY)
		{
			/* Read the position before damaging, so a killing blow still
			   marks where the enemy was standing. */
			const enemy_t* enemy = getEnemy(target->enemyId);

			if(enemy)
				setImpact(enemy->x, enemy->y, IMPACT_FRAME_ENEMY, offsetFrac, fracY);

			damageEnemy(target->enemyId, player.currentWeapon->damage);
		}
		else
		{
			/* A decoration, drawn from the middle of its cell, which is where
			   the marker goes. Then the level says what a shot does to it: it
			   may rewrite the cell, and this frame's sprite list still draws
			   the old one, under the marker, one last time. */
			setImpact(int2fp(target->cellX) + flt2fp(0.5f), int2fp(target->cellY) + flt2fp(0.5f),
				IMPACT_FRAME_ENEMY, offsetFrac, fracY);

			levelEvent(LEVEL_EVENT_SHOOT_DECOR, (u8)(target->spriteId & 7), target->cellX, target->cellY);
		}

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

		/* Horizontally this is already on the real ray, so only the vertical
		   needs scattering - without it every wall hit sits on the horizon. */
		setImpact(player.pos.x + fpmul(f_dx, f_impactDepth),
			player.pos.y + fpmul(f_dy, f_impactDepth),
			IMPACT_FRAME_WALL, 0,
			impactFracY(player.currentWeapon->accuracy));

		/* And whatever the wall itself does about being shot. */
		hitWallCell(f_wallDepth[aimSpan], f_dx, f_dy);
	}
}

/* Drawn after the enemies so a hit marker sits on top of the target rather
   than being painted over by it. */
static void drawImpact(const f16* f_wallDepth, const f16 f_viewCos, const f16 f_viewSin)
{
	spritehit_t hit;
	s16 targetHeight;

	if(impact.framesLeft == 0)
		return;

	impact.framesLeft--;

	if(!projectSprite(impact.x, impact.y, &hit, f_viewCos, f_viewSin))
		return;

	if(hit.f_spriteDist >= f_wallDepth[hit.spanX])
		return;

	/* What the thing that was hit measures on screen, read before the clamp
	   below inflates the mark itself. Scaling the offsets by the clamped size
	   would throw them off a distant target - a far wall may be ten rows tall
	   while the mark is held at sixty. */
	targetHeight = hit.spriteHeight;

	/* Distant impacts would otherwise shrink to a couple of pixels, so hold a
	   floor on the apparent size the way the original wall impact did. */
	if(hit.f_spriteDist > IMPACT_FAR_DEPTH)
		hit.spriteHeight = IMPACT_HEIGHT_NUM / IMPACT_FAR_DEPTH;

	/* Both products stay within s16: the fractions reach 32 and targetHeight
	   960, since projectSprite rejects anything nearer than SPRITE_NEAR_DEPTH.
	   Widening to s32 here would pull in TopSpeed's 32 bit multiply helper. */
	hit.offsetX = (s16)((impact.fracX * targetHeight) >> 6);
	hit.offsetY = (s16)((impact.fracY * targetHeight) >> 6);

	hit.spriteId = (u8)((SPRITE_SLOT_PARTICLES << 3) | impact.frame);
	hit.mirrored = FALSE;
	hit.enemyId = SPRITE_NO_ENEMY;

	drawProjectedSprite(&hit);
}

/* rayDelta() of every sincos_tab entry. Ray directions are always table
   entries, so the divide is hoisted out of the frame entirely. */
static u16 recipTab[TRIG_TABLE_LEN];
static u8 recipReady = FALSE;

static u16 rayDelta(const f16 f_dir)
{
	u16 dir = f_dir < 0 ? -f_dir : f_dir;
	u16 delta;

	if(dir <= 2)
		return FP_MAX;

	/* Match 65536 / dir using a 16-bit dividend. */
	delta = ((u16)0xffff) / dir;

	if((dir & (dir - 1)) == 0)
		delta++;

	return delta;
}

/* A pillar cell holds its wall inside the cell rather than on its faces: a
   square post in the cell's middle. Drawn on the cell faces it came apart,
   two slabs on the cell's edges with a gap at the corner. So when the DDA
   steps into a pillar cell the ray is tested against the post itself, the
   standard slab test, written for any box in the cell. At that point sidedx and
   sidedy are the ray lengths to the far x and y faces of the cell and
   deltax, deltay the lengths per cell, so a box spanning lo..hi in x is
   entered at sidedx - deltax * (256 - lo) / 256 and left at
   sidedx - deltax * (256 - hi) / 256 for a ray travelling +x, and at
   sidedx - deltax * hi / 256 and sidedx - deltax * lo / 256 travelling -x;
   likewise for y. The ray is in the box where the two intervals overlap,
   entering through whichever slab it entered last, which gives the face and
   the distance; wallX along that face falls out of the other coordinate as
   it does for a cell face, scaled up to a whole face so the wall style sees
   0..255 as on any other.

   A ray parallel to an axis has no delta on it (sidedx is DDA_NEVER and the
   subtraction would wrap), but its coordinate on that axis never changes, so
   it is in that slab for the whole cell or not at all. And a ray that starts
   inside a slab's range - the player standing in the same row or column of
   cells as the box - has that slab's entry behind it: the subtraction would
   go negative and wrap, so an entry behind the origin is an entry at zero,
   and an exit behind the origin is a miss.

   A miss lets the DDA carry on out of the cell, as it does today through the
   open part of any non solid cell. A hit is solid: the post has four faces
   and hides what stands behind it. Only run when a ray hits a pillar cell,
   so it costs the DDA loop one compare per wall hit and nothing per step.

   An archway with real jambs was built on the same test and taken out
   again: standing in front of a doorway and looking through, the reveals
   fill both sides of the screen with full height wall at a grazing angle,
   and that measured 13fps against 20 on the device. */

#define PILLAR_LO 96 /* the post, in both axes */
#define PILLAR_HI 160

typedef struct boxhit_t
{
	f16 f_dist;
	u16 side;
	s16 lo;    /* the hit face's start along wallX, to subtract */
	u16 shift; /* and its scale up to a whole face */
} boxhit_t;

static u16 slabHit(const u16 sided, const u16 delta, const s16 step, const f16 f_d,
	const f16 f_pos, const s16 lo, const s16 hi, u16* in, u16* out)
{
	u16 toIn, toOut;

	if(f_d == 0)
	{
		if(f_pos < lo || f_pos >= hi)
			return FALSE;

		*in = 0;
		*out = DDA_NEVER;

		return TRUE;
	}

	if(step > 0)
	{
		toIn = (u16)fpmul((f16)delta, 256 - lo);
		toOut = (u16)fpmul((f16)delta, 256 - hi);
	}
	else
	{
		toIn = (u16)fpmul((f16)delta, hi);
		toOut = (u16)fpmul((f16)delta, lo);
	}

	if(sided <= toOut)
		return FALSE;

	*in = sided > toIn ? sided - toIn : 0;
	*out = sided - toOut;

	return TRUE;
}

static u16 boxHit(const u16 sidedx, const u16 sidedy, const u16 deltax, const u16 deltay,
	const s16 stepx, const s16 stepy, const f16 f_dx, const f16 f_dy,
	const s16 mapx, const s16 mapy,
	const s16 xlo, const s16 xhi, const s16 ylo, const s16 yhi, boxhit_t* box)
{
	u16 xin, xout, yin, yout, in, out;
	s16 w;

	if(!slabHit(sidedx, deltax, stepx, f_dx, player.pos.x - int2fp(mapx), xlo, xhi, &xin, &xout))
		return FALSE;

	if(!slabHit(sidedy, deltay, stepy, f_dy, player.pos.y - int2fp(mapy), ylo, yhi, &yin, &yout))
		return FALSE;

	in = xin > yin ? xin : yin;
	out = xout < yout ? xout : yout;

	/* Equal is a ray through the box's corner, entering one slab as it leaves
	   the other. That is a hit on the corner: called a miss, the ray steps on
	   into the solid cell beyond and draws a face of it that is buried inside
	   the box, a column of the wrong shade bleeding through. */
	if(in > out)
		return FALSE;

	box->f_dist = (f16)in;
	box->side = xin > yin ? 0 : 1;

	/* An x face runs along y, so wallX on it is the y coordinate. */
	if(box->side == 0)
	{
		box->lo = ylo;
		w = yhi - ylo;
	}
	else
	{
		box->lo = xlo;
		w = xhi - xlo;
	}

	box->shift = 0;

	while(w < 256)
	{
		w <<= 1;
		box->shift++;
	}

	return TRUE;
}

void draw()
{
	u16 i;
	u16 spritesMarked = 0;
	u16 spritesHit = 0;
	u16 visibleSprites;
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

		/* Unsigned so the accumulators cannot wrap. A near axis-parallel ray
		   has a delta of up to FP_MAX, which overflows s16 the first time it
		   is added to a side distance the ray has already carried across the
		   map; the wrapped value then stays below the other axis for the rest
		   of the cast and the ray runs off sideways. */
		u16 sidedx, sidedy;
		
		u16 side;
		
		u16 solid, hits;

		boxhit_t box;
	
		const s16 rayIdx = baseIdx + rayIdxOffset[i];
		const u16 cosIdx = (u16)((rayIdx + TRIG_COS_OFFSET) & TRIG_TABLE_MASK);
		const u16 sinIdx = (u16)(rayIdx & TRIG_TABLE_MASK);

		const f16 f_dx = sincos_tab[cosIdx];
		const f16 f_dy = sincos_tab[sinIdx];

		const u16 deltax = recipTab[cosIdx];
		const u16 deltay = recipTab[sinIdx];

		f_wallDepth[i] = FP_MAX;


		if(f_dx < 0)
		{
			stepx = -1;
			sidedx = fpmul(player.pos.x - int2fp(mapx), (f16)deltax);
		}
		else
		{
			stepx = 1;
			sidedx = fpmul(int2fp(mapx + 1) - player.pos.x, (f16)deltax);
		}

		if(f_dy < 0)
		{
			stepy = -1;
			sidedy = fpmul(player.pos.y - int2fp(mapy), (f16)deltay);
		}
		else
		{
			stepy = 1;
			sidedy = fpmul(int2fp(mapy + 1) - player.pos.y, (f16)deltay);
		}

		/* A ray straight down an axis never crosses a boundary on the other one,
		   but rayDelta() has no infinity to return and caps the delta at FP_MAX
		   instead - and fpmul above then scales that cap down by the player's
		   distance to the grid line. Standing a 256th of a cell from it turns
		   "never" into 127, near enough that the DDA steps sideways off the ray.
		   Say never directly. Kept out of the branches above so their code
		   generation, and the register pressure around fpmul, is untouched. */
		if(f_dx == 0)
			sidedx = DDA_NEVER;

		if(f_dy == 0)
			sidedy = DDA_NEVER;
		
		solid = 0;
		hits = 0;
		
		do
		{
			u16 hit;
			u16 hitcell;
			f16 f_dist;
			f16 f_wallx;
			f16 f_step;
			u16 delta;

			for(;;)
			{
				do
				{
					if(sidedx < sidedy)
					{
						sidedx = sidedx + deltax;
						mapx += stepx;
						side = 0;
					}
					else
					{
						sidedy = sidedy + deltay;
						mapy += stepy;
						side = 1;
					}

					hitcell = mapCell(mapx, mapy);
					hit = isWall(hitcell);

					/* Sprites are gathered from every cell the ray can see into, which
					   is every cell that is not solid - an enemy standing in an archway
					   or doorway is in a wall cell and used to be skipped here, so it
					   simply was not drawn. Same single mask test as the old hit == 0. */
					if(!isSolid(hitcell))
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
									/* A non enemy sprite cell is a pickup or a decoration. The
									   low three bits of the type nibble are the frame within the
									   slot, and DECOR_TYPE_BIT says which slot that is. */
									const u16 type = GET_CELL_TYPE_ID(hitcell);
									const u8 slot = (type & DECOR_TYPE_BIT) ? SPRITE_SLOT_DECORATIONS : SPRITE_SLOT_PICKUPS;

									spriteHits[spritesHit].spriteId = (u8)((slot << 3) | (type & 7));
									spriteHits[spritesHit].mirrored = FALSE;
									spriteHits[spritesHit].enemyId = SPRITE_NO_ENEMY;
									spriteHits[spritesHit].cellX = (u8)mapx;
									spriteHits[spritesHit].cellY = (u8)mapy;
									spritesHit++;
								}
							}
						}
					}
				} while(hit == 0);

				/* A pillar cell's wall is the post in its middle, not its faces:
				   hit 2 is a hit on the post, a miss sends the ray on. Tested
				   here, once per wall hit, and not in the step loop above: a
				   compare per DDA step measured a frame a second. */
				if(mapCellType(hitcell) == WALL_TYPE_PILLAR)
				{
					if(!boxHit(sidedx, sidedy, deltax, deltay, stepx, stepy, f_dx, f_dy,
						mapx, mapy, PILLAR_LO, PILLAR_HI, PILLAR_LO, PILLAR_HI, &box))
						continue;

					hit = 2;
				}

				break;
			}
			
			solid = isSolid(hitcell) || hits >= 2 || hit == 2;
			
			wallhits[hits].cell = hitcell;
			wallhits[hits].mapX = (u8)mapx;
			wallhits[hits].mapY = (u8)mapy;
			wallhits[hits].side = side;

			/* The side distance was advanced past the boundary just crossed, so
			   subtracting one delta recovers the distance to it, without the
			   bit-serial 32 bit divide the old fpdiv needed. Exact for every ray
			   now that the accumulators are unsigned: the difference is the
			   distance the ray has actually travelled, so it fits f16 whatever
			   the delta was. */
			if(hit == 2)
			{
				/* A box face: wallX along it is scaled up to a whole face so the
				   wall style sees 0..255 as on any other, and the footprint below
				   is scaled with it. */
				f_dist = box.f_dist;
				side = box.side;
				wallhits[hits].side = (u8)side;

				if(side == 0)
				{
					f_wallx = player.pos.y + fpmul(f_dist, f_dy) - int2fp(mapy) - box.lo;
					delta = deltax;
				}
				else
				{
					f_wallx = player.pos.x + fpmul(f_dist, f_dx) - int2fp(mapx) - box.lo;
					delta = deltay;
				}

				f_wallx <<= box.shift;
			}
			else if(side == 0)
			{
				f_dist = (f16)(sidedx - deltax);

				f_wallx = player.pos.y + fpmul(f_dist, f_dy);
				delta = deltax;
			}
			else
			{
				f_dist = (f16)(sidedy - deltay);

				f_wallx = player.pos.x + fpmul(f_dist, f_dx);
				delta = deltay;
			}

			if(f_dist <= 0)
				f_dist = 1;

			/* How much of the face this column covers, either side of the
			   sample. wallX moves along the face by dist / cos per radian, and
			   delta is already 1 / cos; the column's half width in radians is
			   rayHalfWidth, in 4096ths, so the product is (f_step / 16 * w)
			   / 256. The margin on top is on purpose: it is a footprint for
			   feature tests, and neighbouring footprints must overlap rather
			   than leave gaps, or a feature falling in a gap is lost exactly
			   as it was under a point test. f_wallx itself is only good to
			   about 1% of its offset from the player - delta is floored, f_dy
			   is rounded, and fpmul truncates twice - so f_step / 128 covers
			   that, and the plus one keeps the footprint from reaching 0 on a
			   wall the player is touching. A feature can therefore land in two
			   neighbouring columns, and there is no way for a column to tell
			   which of them it is; wall styles draw such features a column
			   wide and accept the doubling.
			   Per hit, not per DDA step. Clamped to a whole face: fpmul keeps
			   bits 8..23 of the product, so a grazing hit at over 64 cells
			   along the ray runs the result up towards the sign bit, and past
			   256 cells wraps it. The clamp also bounds the 16 bit product
			   below: 1024 * 50 fits. */
			f_step = fpmul(f_dist, (f16)delta);

			if(hit == 2)
				f_step = (u16)f_step >= (16384 >> box.shift) ? 16384 : (f_step << box.shift);

			if((u16)f_step >= 16384)
				f_step = 16384;

			wallhits[hits].wallHeight = WALL_HEIGHT_NUM / f_dist;
			wallhits[hits].f_wallDist = f_dist;
			wallhits[hits].f_wallX = f_wallx - int2fp(fp2int(f_wallx));
			wallhits[hits].f_wallXHalf = (s16)((((u16)f_step >> 4) * rayHalfWidth[i]) >> 8) + (f_step >> 7) + 1;
			
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

	visibleSprites = spritesHit;

	while(spritesHit > 0)
	{
		spritesHit--;

		if(spriteHits[spritesHit].f_spriteDist < f_wallDepth[spriteHits[spritesHit].spanX])
			drawProjectedSprite(&spriteHits[spritesHit]);
	}

	drawImpact(f_wallDepth, f_viewCos, f_viewSin);
	drawEnemyTracers(spriteHits, visibleSprites, f_wallDepth);
	
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

	drawCrosshair();

	/* Hurt flash: a thick black bar on the screen edge nearest the shooter,
	   over everything, for a few frames. On a white background black is the
	   loud colour, and the side it lands on says which way to turn. Counted
	   down here because it is a per frame effect, not a per tick one. */
	if(player.hitFlash > 0)
	{
		player.hitFlash--;

		if(player.hitDir & PLAYER_HIT_FRONT)
			bmFillRect(0, 0, 240, HIT_FLASH_THICKNESS, blackBm);
		else if(player.hitDir & PLAYER_HIT_BACK)
			bmFillRect(0, 160 - HIT_FLASH_THICKNESS, 240, HIT_FLASH_THICKNESS, blackBm);
		else if(player.hitDir & PLAYER_HIT_LEFT)
			bmFillRect(0, 0, HIT_FLASH_THICKNESS, 160, blackBm);
		else if(player.hitDir & PLAYER_HIT_RIGHT)
			bmFillRect(240 - HIT_FLASH_THICKNESS, 0, HIT_FLASH_THICKNESS, 160, blackBm);
	}

	//Add rect around screen.
	bmDrawRect(0, 0, 240, 160, blackBm);

}
