#include "enemy.h"
#include "sprslot.h"
#include "game_map.h"
#include "psion3d.h"
#include "units.h"
#include "draw.h"
#include "pickup.h"

#define ENEMY_LEASH_DIST_METERS 28
#define ENEMY_ATTACK_DIST_MER_METERS 4
#define ENEMY_ATTACK_DIST_SGR_METERS 6
#define ENEMY_ATTACK_DIST_HVY_METERS 8
/* How close is too close. The Soldier keeps its distance and gives ground when
   the player closes. Nobody else does: a Merc has no spacing discipline and
   fires from wherever it ends up, and a Heavy plants - backing off at 0.5 m/s
   took four seconds during which it could be kited for free. Distance is
   Manhattan cells and never 0, so one cell means never retreat. */
#define ENEMY_ATTACK_MIN_DIST_METERS 2
#define ENEMY_ATTACK_MIN_DIST_SGR_METERS 4
#define ENEMY_CIV_AVOID_DIST_METERS 6
#define ENEMY_FLEE_SAFE_DIST_METERS 12
#define ENEMY_ALERT_DIST_METERS 20

#define ENEMY_LEASH_DIST METERS_TO_MAP_CELLS(ENEMY_LEASH_DIST_METERS)
#define ENEMY_ATTACK_DIST_MER METERS_TO_MAP_CELLS(ENEMY_ATTACK_DIST_MER_METERS)
#define ENEMY_ATTACK_DIST_SGR METERS_TO_MAP_CELLS(ENEMY_ATTACK_DIST_SGR_METERS)
#define ENEMY_ATTACK_DIST_HVY METERS_TO_MAP_CELLS(ENEMY_ATTACK_DIST_HVY_METERS)
#define ENEMY_ATTACK_MIN_DIST METERS_TO_MAP_CELLS(ENEMY_ATTACK_MIN_DIST_METERS)
#define ENEMY_ATTACK_MIN_DIST_SGR METERS_TO_MAP_CELLS(ENEMY_ATTACK_MIN_DIST_SGR_METERS)
#define ENEMY_CIV_AVOID_DIST METERS_TO_MAP_CELLS(ENEMY_CIV_AVOID_DIST_METERS)
#define ENEMY_FLEE_SAFE_DIST METERS_TO_MAP_CELLS(ENEMY_FLEE_SAFE_DIST_METERS)
#define ENEMY_ALERT_DIST METERS_TO_MAP_CELLS(ENEMY_ALERT_DIST_METERS)

/* Aim and attack timing live in enemyStats[] per type. */
#define ENEMY_IDLE_DELAY SECONDS_TO_TICKS(1)
#define ENEMY_SURPRISED_DELAY fpSecondsToTicks(flt2fp(0.5f))
#define ENEMY_EVADE_DELAY fpSecondsToTicks(flt2fp(0.4f))

/* A sidestep is a burst, not a walk: the same short glide for every type, so
   an evade or a shift between shots costs a Heavy the same half second it costs
   a Merc, not the four seconds of its walking pace. Retreats and flight are
   walks and keep the per-type move period. */
#define ENEMY_SIDESTEP_TICKS 16

/* Ticks the firing frame shows after each round of a burst; the aim frame
   fills the rest of the interval. Without this the whole burst wore the
   muzzle flash, which on a Heavy meant a flash that never went out. */
#define ENEMY_FLASH_TICKS 4

/* How long an enemy keeps its aim on a player who ducks out of sight. Reappear
   within this and the wind-up resumes where it left off rather than starting
   over, so cover is a decision instead of a loop - peek-and-shoot works once,
   then the gun is already up. A resumed aim is never shorter than the
   reacquire floor, so it is never literally instant. */
#define ENEMY_AIM_MEMORY_TICKS 64
#define ENEMY_REACQUIRE_TICKS 4
#define ENEMY_HURT_DELAY fpSecondsToTicks(flt2fp(0.25f))
#define ENEMY_DYING_DELAY fpSecondsToTicks(flt2fp(0.3f))

//How long the corpse lies there before the weapon it carried takes its place.
#define ENEMY_DROP_DELAY SECONDS_TO_TICKS(1)

//Move periods of flight before a fleeing enemy starts testing whether it is safe.
#define ENEMY_FLEE_CELLS 6

/* Move periods a search lasts, covering both the walk to the last known
   position and whatever sweeping is left over once it gets there. */
#define ENEMY_SEARCH_CELLS 20

//0..255 chance of stopping to idle between wander steps.
#define ENEMY_WANDER_PAUSE_CHANCE 64

//0..255 chance of turning 90 degrees at a wander step.
#define ENEMY_WANDER_TURN_CHANCE 48

#define ENEMY_COLLISION_RADIUS flt2fp(0.75f)
#define ENEMY_COLLISION_RADIUS_SQ ((s32)ENEMY_COLLISION_RADIUS * ENEMY_COLLISION_RADIUS)

const enemystats_t enemyStats[] =
{
	/* Stagger: 0 flinches at every hit. The Heavy's 20 lets the pistol (40) and
	   LMG (20) rock it while SMG (15) and AK (16) rounds land without a flinch.
	   Fire is in bursts: aim is the wind-up, atk the interval between rounds,
	   and repos the chance each round ends the burst with a sidestep - so it
	   sets burst length and movement together. Merc: 0.75s wind-up, then
	   rounds a quarter second apart in bursts of about two. Soldier and Heavy
	   still carry the pre-burst placeholders. */
	//                 evade flee hp   stag dmg acc aim atk repos
	{flt2fp(1),    48, 240, 100, 0,  0,  0,  16, 32, 0,   SPRITE_SLOT_CIV}, //Civilian
	{flt2fp(1.5),  32, 16,  75,  0,  10, 24, 24, 8,  128, SPRITE_SLOT_MER}, //Mercenary
	{flt2fp(2),    64, 8,   150, 0,  10, 25, 16, 10, 64,  SPRITE_SLOT_SGR}, //Soldier
	{flt2fp(0.5),  16, 4,   255, 20, 20, 28, 32, 13, 32,  SPRITE_SLOT_HVY}  //Heavy, u8 ceiling
};


#define ENEMY_TYPE_COUNT 4

/* Derived once per map load from enemyStats[].moveSpeed. */
static u8 enemyMoveTickTable[ENEMY_TYPE_COUNT];

enemy_t enemyList[MAX_ENEMIES];

u16 enemyCount = 0;

static u16 enemyRand = 0x9a31;

/* The player's map cell, captured once at the top of runAI. The player does
   not move during an AI tick, and every distance, sight and target test wants
   the same pair, so this saves recomputing it per enemy per helper. Only valid
   inside runAI. */
static s16 playerCellX = 0;
static s16 playerCellY = 0;

static u16 absDiff(const s16 a, const s16 b)
{
    return a > b ? a - b : b - a;
}

static void enemySetMoveTarget(enemy_t* enemy, const s16 x, const s16 y);

/* An enemy is an overlay on whatever it stands on. The cell keeps its own
   flags and type nibble - floor, pickup, archway or doorway - and gains the
   enemy marker on top, so leaving can restore it exactly. The renderer reads
   the enemy's sprite from enemyList by id, never from the cell's type. */
static u16 enemyOverlayCell(const u16 id, const enemy_t* enemy)
{
    return (enemy->underCell | MAP_MASK_SPRITE | MAP_MASK_ENEMY | id);
}

static u16 enemyDistanceToPlayer(const enemy_t* enemy)
{
    return absDiff(fp2int(enemy->x), playerCellX) + absDiff(fp2int(enemy->y), playerCellY);
}

static u16 enemyAttackDistance(const enemy_t* enemy)
{
    switch(enemy->type)
    {
        case ENEMY_TYPE_SGR:
            return ENEMY_ATTACK_DIST_SGR;

        case ENEMY_TYPE_HVY:
            return ENEMY_ATTACK_DIST_HVY;
    }

    return ENEMY_ATTACK_DIST_MER;
}

/* Closer than this an enemy gives ground before aiming, if it can. Paired
   with enemyAttackDistance it defines the firing band. Only the Soldier
   spaces; see the defines for why the others hold their ground. */
static u16 enemyAttackMinDistance(const enemy_t* enemy)
{
    if(enemy->type == ENEMY_TYPE_SGR)
        return ENEMY_ATTACK_MIN_DIST_SGR;

    return ENEMY_ATTACK_MIN_DIST;
}

static void enemyUpdateMapCell(const u16 id, enemy_t* enemy)
{
    u8 newX = (u8)fp2int(enemy->x);
    u8 newY = (u8)fp2int(enemy->y);
    u16 oldCell;
    u16 newCell;

    if(newX == enemy->cellX && newY == enemy->cellY)
        return;

    newCell = mapCell(newX, newY);

    /* Somebody else got here first. Two enemies can pick the same empty cell
       in one tick; the move test only ran when the step was chosen, and the
       cell is claimed on crossing. Overwriting would capture their marker as
       our underCell and resurrect it as a ghost when we left, so turn back
       instead: the same counter carries the glide home. */
    if(isEnemy(newCell))
    {
        enemySetMoveTarget(enemy, enemy->cellX, enemy->cellY);
        return;
    }

    oldCell = mapCell(enemy->cellX, enemy->cellY);

    if(isEnemy(oldCell) && GET_CELL_ID(oldCell) == id)
        updateCell(enemy->cellX, enemy->cellY, enemy->underCell);

    /* MARKED is per frame scratch for the renderer; never carry it. */
    enemy->underCell = newCell & ~MAP_MASK_MARKED;
    updateCell(newX, newY, enemyOverlayCell(id, enemy));

    enemy->cellX = newX;
    enemy->cellY = newY;
}

/* Orthogonal neighbours, for finding floor beside a corpse. */
static const s8 neighbourX[4] = {1, -1, 0, 0};
static const s8 neighbourY[4] = {0, 0, 1, -1};

/* The corpse is drawn from its map cell, so giving the cell back is what
   removes the body. Bodies do not linger: a corpse holds the cell's enemy
   marker, and the cell format has no room for a second enemy on top, so a
   permanent body is a permanent obstacle - one kill in a corridor used to stop
   everything behind it. Combat types leave their weapon behind. */
static void enemyReleaseCell(const u16 id, const enemy_t* enemy)
{
    u16 cell;
    u8 pickupType;
    u8 i;

    /* The release must never touch a cell that has stopped being this corpse. */
    cell = mapCell(enemy->cellX, enemy->cellY);

    if(!isEnemy(cell) || GET_CELL_ID(cell) != id)
        return;

    updateCell(enemy->cellX, enemy->cellY, enemy->underCell);

    switch(enemy->type)
    {
        case ENEMY_TYPE_MER:
            pickupType = PICKUP_TYPE_MP5;
            break;

        case ENEMY_TYPE_SGR:
            pickupType = PICKUP_TYPE_AK47;
            break;

        case ENEMY_TYPE_HVY:
            pickupType = PICKUP_TYPE_M249;
            break;

        default: //Civilians carry nothing.
            return;
    }

    /* A pickup owns the cell's type nibble, so it can only go on bare floor.
       Dying in an archway or doorway drops the weapon beside the opening. */
    if(enemy->underCell == MAP_MASK_WALK)
    {
        updateCell(enemy->cellX, enemy->cellY, makePickupCell(pickupType));
        return;
    }

    for(i = 0; i < 4; i++)
    {
        s16 x = enemy->cellX + neighbourX[i];
        s16 y = enemy->cellY + neighbourY[i];

        if(x >= 0 && y >= 0 && x < MAP_X && y < MAP_Y && mapCell(x, y) == MAP_MASK_WALK)
        {
            updateCell(x, y, makePickupCell(pickupType));
            return;
        }
    }
}

/* The view sin/cos are the same for every enemy in a tick, and the angle only
   changes in updatePlayer, so cache them rather than recomputing per enemy. */
static f16 f_viewTrigAngle = 0;
static f16 f_viewSin = 0;
static f16 f_viewCos = 0;
static u8 viewTrigValid = FALSE;

static void enemyUpdateViewTrig()
{
    if(viewTrigValid && f_viewTrigAngle == player.pos.angle)
        return;

    f_viewTrigAngle = player.pos.angle;
    f_viewSin = fpsin(f_viewTrigAngle);
    f_viewCos = fpcos(f_viewTrigAngle);
    viewTrigValid = TRUE;
}

static void enemySetWalkFrame(enemy_t* enemy)
{
    f16 f_dx = enemy->moveTargetX - enemy->x;
    f16 f_dy = enemy->moveTargetY - enemy->y;
    f16 f_side;

    enemyUpdateViewTrig();

    f_side =
        -fpmul(f_dx, f_viewSin) +
         fpmul(f_dy, f_viewCos);

    /* Both cases pick the same frame; only the mirroring differs. */
    enemy->spriteFrame = (enemy->stateCounter & 1) ? ENEMY_FRAME_WALK_R1 : ENEMY_FRAME_WALK_R2;
    enemy->spriteMirrored = (f_side > 0) ? FALSE : TRUE;
}

static void enemyMoveTick(const u16 id, enemy_t* enemy)
{
    if(enemy->x == enemy->moveTargetX && enemy->y == enemy->moveTargetY)
        return;

    enemySetWalkFrame(enemy);

    if(enemy->stateCounter == 0)
    {
        enemy->x = enemy->moveTargetX;
        enemy->y = enemy->moveTargetY;
        enemyUpdateMapCell(id, enemy);
        return;
    }

    /* This was fpdiv(a, int2fp(n)), which is (a << 8) / (n << 8) - the shifts
       cancel, so it is only a / n. Written directly it is a native 16 bit
       divide instead of the called bit-serial 32 bit one, and it drops a latent
       bug: int2fp(n) overflows f16 once n passes 127, so the old form went
       negative for any enemy slower than 1 m/s. stateCounter is non-zero here,
       checked just above. */
    enemy->x += (enemy->moveTargetX - enemy->x) / enemy->stateCounter;
    enemy->y += (enemy->moveTargetY - enemy->y) / enemy->stateCounter;
    enemyUpdateMapCell(id, enemy);
}

static u16 enemyCounterTick(const u16 id, enemy_t* enemy)
{
    if(enemy->stateCounter == 0)
        return FALSE;

    enemyMoveTick(id, enemy);
    enemy->stateCounter--;

    return TRUE;
}

static void enemySetMoveCounter(enemy_t* enemy, const u8 ticks)
{
    enemy->stateCounter = ticks;

    if(enemy->x != enemy->moveTargetX || enemy->y != enemy->moveTargetY)
        enemySetWalkFrame(enemy);
    else
        enemy->spriteFrame = ENEMY_FRAME_IDLE;
}

static void enemySetMoveTarget(enemy_t* enemy, const s16 x, const s16 y)
{
    enemy->moveTargetX = int2fp(x) + flt2fp(0.5f);
    enemy->moveTargetY = int2fp(y) + flt2fp(0.5f);
}

/* Stop where the enemy currently stands, mid cell if need be.

   enemyMoveTick covers the *remaining* distance in whatever stateCounter now
   holds, so anything that shortens the counter part way through a move silently
   speeds that move up. damageEnemy setting the 8 tick hurt delay over a 128 tick
   heavy stride made it finish the cell 16 times too fast, which reads as the
   enemy warping. Dropping the target instead leaves it standing where it was
   hit, and the map cell already tracks this position from the last move tick. */
static void enemyCancelMove(enemy_t* enemy)
{
    enemy->moveTargetX = enemy->x;
    enemy->moveTargetY = enemy->y;
}

static u8 enemyRandomBit()
{
    enemyRand = (u16)(enemyRand * 17 + 43);

    return (u8)((enemyRand >> 8) & 1);
}

static u8 enemyRandomChance(const u8 chance)
{
    enemyRand = (u16)(enemyRand * 17 + 43);

    return (u8)(enemyRand >> 8) < chance;
}

static u8 enemyMoveTicks(const enemy_t* enemy)
{
    return enemyMoveTickTable[enemy->type];
}

/* State transitions that are entered from more than one place. Keeping each
   in one function stops the copies drifting apart - the search entry in
   particular was written out three times with two different counters. */

/* delayTicks is how long to stand before the first search step: a move period
   from the chase, so losing sight reads as a pause, or 0 from an alert, so the
   reaction is immediate. */
static void enemyStartSearch(enemy_t* enemy, const u8 delayTicks)
{
    enemy->state = ENEMY_STATE_SEARCHING;
    enemy->stateCells = ENEMY_SEARCH_CELLS;
    enemy->stateCounter = delayTicks;
}

static void enemyStartFlee(enemy_t* enemy)
{
    enemy->state = ENEMY_STATE_FLEEING;
    enemy->stateCells = ENEMY_FLEE_CELLS;
    enemy->stateCounter = 0;
}

static void enemyStartAim(enemy_t* enemy)
{
    enemy->state = ENEMY_STATE_AIMING;
    enemy->spriteFrame = ENEMY_FRAME_AIM;

    /* Sight lost and regained inside the memory window: pick the aim up
       where it was, and spend the memory so a later loss starts afresh. */
    if(enemy->aimMemoryAge < ENEMY_AIM_MEMORY_TICKS)
    {
        enemy->stateCounter = enemy->aimMemory;
        enemy->aimMemoryAge = ENEMY_AIM_MEMORY_TICKS;
        return;
    }

    enemy->stateCounter = enemy->enemyStats->aimTicks;
}

/* Sight lost part way through an aim or a burst. Remember how much aim was
   still owed - none, for a burst already under way - and go looking. */
static void enemyLoseAim(enemy_t* enemy, const u8 remaining)
{
    enemy->aimMemory = (remaining < ENEMY_REACQUIRE_TICKS) ? ENEMY_REACQUIRE_TICKS : remaining;
    enemy->aimMemoryAge = 0;
    enemyStartSearch(enemy, enemyMoveTicks(enemy));
}

/* Something loud happened at x, y. Only enemies that are not already dealing
   with the player care: idle ones go and look, wandering civilians run, and one
   already searching redirects to the newer noise.

   Deliberately distance only, with no line of sight test - sound goes round
   corners, and it keeps this off the Bresenham path entirely. */
void alertEnemies(const u8 x, const u8 y)
{
    u16 id;

    for(id = 0; id < enemyCount; id++)
    {
        enemy_t* enemy = &enemyList[id];

        if(enemy->state != ENEMY_STATE_IDLE &&
           enemy->state != ENEMY_STATE_WANDER &&
           enemy->state != ENEMY_STATE_SEARCHING)
            continue;

        if((absDiff(fp2int(enemy->x), x) + absDiff(fp2int(enemy->y), y)) > ENEMY_ALERT_DIST)
            continue;

        enemy->targetX = x;
        enemy->targetY = y;

        /* Both entries zero the counter, which abandons any move in flight. The
           next step interpolates from wherever the enemy stopped, so this reads
           as a reaction rather than a jump. Civilians run from trouble;
           everyone else goes to find it. */
        if(enemy->type == ENEMY_TYPE_CIV)
            enemyStartFlee(enemy);
        else
            enemyStartSearch(enemy, 0);
    }
}

static void enemyShootPlayer(const u16 id, const enemy_t* enemy)
{
    u8 damage;
    u8 onTarget;

    /* A shot is loud whether or not it connects, and this is what carries the
       alarm outward - the player's shot wakes the first room, those enemies
       firing back wake the next one. */
    alertEnemies((u8)fp2int(enemy->x), (u8)fp2int(enemy->y));

    onTarget = enemyRandomChance(enemy->enemyStats->accuracy) && player.health > 0;

    /* Every round leaves a streak, so a near miss reads as a near miss rather
       than as nothing having happened. */
    addEnemyTracer((u8)id, onTarget ? TRACER_AIM_HIT :
        (enemyRandomBit() ? TRACER_AIM_WIDE_L : TRACER_AIM_WIDE_R));

    if(!onTarget)
        return;

    damage = enemy->enemyStats->damage;

    hurtPlayer(damage, enemy->x, enemy->y);
}

/* Mirrors what the renderer lets the player see through. A solid wall stops
   the ray; a non-solid wall - archway, window, bars, low wall, pillar - is
   drawn but seen past. Doors are the one case with state: shut unless someone
   is at them. Locked doors, and any wall type not listed, are shut. */
static u16 cellBlocksSight(const u16 cell, const s16 x, const s16 y)
{
    if(!isWall(cell))
        return FALSE;

    if(isSolid(cell))
        return TRUE;

    switch(mapCellType(cell))
    {
        case WALL_TYPE_ARCH:
        case WALL_TYPE_WINDOW:
        case WALL_TYPE_BARS:
        case WALL_TYPE_LOW:
        case WALL_TYPE_PILLAR:
            return FALSE;

        case WALL_TYPE_UNLOCKED_DOOR:
            return !(doorEnemyNear(x, y) ||
                (absDiff(x, playerCellX) + absDiff(y, playerCellY)) <= 1);
    }

    return TRUE;
}

static u16 enemyCanSeePlayer(const enemy_t* enemy)
{
    s16 x0 = fp2int(enemy->x);
    s16 y0 = fp2int(enemy->y);
    s16 x1 = playerCellX;
    s16 y1 = playerCellY;
    s16 dx = absDiff(x0, x1);
    s16 dy = absDiff(y0, y1);
    s16 sx = x0 < x1 ? 1 : -1;
    s16 sy = y0 < y1 ? 1 : -1;
    s16 err = dx - dy;
    s16 e2;

    while(x0 != x1 || y0 != y1)
    {
        e2 = err << 1;

        if(e2 > -dy)
        {
            err -= dy;
            x0 += sx;
        }

        if(e2 < dx)
        {
            err += dx;
            y0 += sy;
        }

        if(x0 == x1 && y0 == y1)
            return TRUE;

        if(cellBlocksSight(mapCell(x0, y0), x0, y0))
            return FALSE;
    }

    return TRUE;
}

static u16 enemyTryMoveTo(const u16 id, enemy_t* enemy, const s16 newX, const s16 newY)
{
    u16 cell;

    if(newX < 0 || newY < 0 || newX >= MAP_X || newY >= MAP_Y)
        return FALSE;

    /* Anything walkable that no other enemy holds: floor, a pickup, an archway,
       an open doorway. This used to demand bare floor exactly, which made
       every dropped weapon and every opening a wall to the AI. The player is
       not in the map, so their cell has to be excluded by hand or a sidestep
       can glide straight onto them. */
    cell = mapCell(newX, newY);

    if(!canWalk(cell) || isEnemy(cell))
        return FALSE;

    if(newX == playerCellX && newY == playerCellY)
        return FALSE;

    enemySetMoveTarget(enemy, newX, newY);

    return TRUE;
}

/* Returns FALSE when every candidate was blocked and the enemy did not move. */
static u16 enemyStepToward(const u16 id, enemy_t* enemy, const u8 targetX, const u8 targetY)
{
    s16 enemyX = fp2int(enemy->x);
    s16 enemyY = fp2int(enemy->y);
    s16 dx = targetX - enemyX;
    s16 dy = targetY - enemyY;
    s16 stepX = dx < 0 ? -1 : 1;
    s16 stepY = dy < 0 ? -1 : 1;
    s16 slide;

    if(dx == 0 && dy == 0)
        return FALSE;

    if(absDiff(enemyX, targetX) >= absDiff(enemyY, targetY))
    {
        if(dx != 0 && enemyTryMoveTo(id, enemy, enemyX + stepX, enemyY))
            return TRUE;

        if(dy != 0 && enemyTryMoveTo(id, enemy, enemyX, enemyY + stepY))
            return TRUE;
    }
    else
    {
        if(dy != 0 && enemyTryMoveTo(id, enemy, enemyX, enemyY + stepY))
            return TRUE;

        if(dx != 0 && enemyTryMoveTo(id, enemy, enemyX + stepX, enemyY))
            return TRUE;
    }

    /* Every step that closes on the target is blocked. Without this the enemy
       stands still and retries the same blocked cell every move period, which
       stalls indefinitely behind anything that does not move by itself - a
       corpse, or another enemy sitting in AIMING.

       Only slide when one axis is already lined up, so the sidestep is square
       to the approach and gives up no ground. When both axes are off the two
       remaining cells are pure retreats, and taking one only walks back in on
       the next period, so standing put is the better-looking outcome there.

       Random side, otherwise a group all slides the same way and re-jams. */
    slide = enemyRandomBit() ? 1 : -1;

    if(dx == 0)
    {
        if(enemyTryMoveTo(id, enemy, enemyX + slide, enemyY))
            return TRUE;

        return enemyTryMoveTo(id, enemy, enemyX - slide, enemyY);
    }

    if(dy == 0)
    {
        if(enemyTryMoveTo(id, enemy, enemyX, enemyY + slide))
            return TRUE;

        return enemyTryMoveTo(id, enemy, enemyX, enemyY - slide);
    }

    return FALSE;
}

static void enemyStepAwayFrom(const u16 id, enemy_t* enemy, const u8 targetX, const u8 targetY)
{
    s16 enemyX = fp2int(enemy->x);
    s16 enemyY = fp2int(enemy->y);
    s16 dx = enemyX - targetX;
    s16 dy = enemyY - targetY;
    s16 stepX = dx < 0 ? -1 : 1;
    s16 stepY = dy < 0 ? -1 : 1;

    if(dx == 0 && dy == 0)
        return;

    if(absDiff(enemyX, targetX) >= absDiff(enemyY, targetY))
    {
        if(dx != 0 && enemyTryMoveTo(id, enemy, enemyX + stepX, enemyY))
            return;

        if(dy != 0)
            enemyTryMoveTo(id, enemy, enemyX, enemyY + stepY);
    }
    else
    {
        if(dy != 0 && enemyTryMoveTo(id, enemy, enemyX, enemyY + stepY))
            return;

        if(dx != 0)
            enemyTryMoveTo(id, enemy, enemyX + stepX, enemyY);
    }
}

static void enemyStepSideways(const u16 id, enemy_t* enemy, const u8 targetX, const u8 targetY)
{
    s16 enemyX = fp2int(enemy->x);
    s16 enemyY = fp2int(enemy->y);
    s16 dx = targetX - enemyX;
    s16 dy = targetY - enemyY;
    s16 stepX;
    s16 stepY;

    if(dx == 0 && dy == 0)
        return;

    if(absDiff(enemyX, targetX) >= absDiff(enemyY, targetY))
    {
        stepX = 0;
        stepY = enemyRandomBit() ? 1 : -1;
    }
    else
    {
        stepX = enemyRandomBit() ? 1 : -1;
        stepY = 0;
    }

    if(enemyTryMoveTo(id, enemy, enemyX + stepX, enemyY + stepY))
        return;

    enemyTryMoveTo(id, enemy, enemyX - stepX, enemyY - stepY);
}

/* The state an enemy returns to once it has nothing else to react to. Civilians
   have no combat states, so this is the one place that keeps them out of the
   chase loop - every path back to "normal" goes through here. */
static u8 enemyAlertState(const enemy_t* enemy)
{
    return (enemy->type == ENEMY_TYPE_CIV) ? ENEMY_STATE_WANDER : ENEMY_STATE_CHASING;
}

/* Cardinal steps indexed by enemy->wanderDir. Turning is +/-1 modulo 4, so the
   order has to walk around the compass rather than pair up opposites. */
static const s8 wanderStepX[4] = {1, 0, -1, 0};
static const s8 wanderStepY[4] = {0, 1, 0, -1};

static void enemyWanderStep(const u16 id, enemy_t* enemy)
{
    s16 enemyX = fp2int(enemy->x);
    s16 enemyY = fp2int(enemy->y);
    u8 dir = enemy->wanderDir;
    u8 d;
    u8 i;

    /* Mostly carry on in the current heading, so wandering reads as walking
       somewhere rather than as a drunk random walk. */
    if(enemyRandomChance(ENEMY_WANDER_TURN_CHANCE))
        dir = (u8)((dir + (enemyRandomBit() ? 1 : 3)) & 3);

    /* Preferred heading first, then the two turns, then the reverse - so a dead
       end still turns the enemy around instead of stalling it. */
    for(i = 0; i < 4; i++)
    {
        d = (u8)((dir + i) & 3);

        if(enemyTryMoveTo(id, enemy, enemyX + wanderStepX[d], enemyY + wanderStepY[d]))
        {
            enemy->wanderDir = d;
            return;
        }
    }
}

/* Back off one cell, sliding sideways along a wall when the direct retreat is
   blocked. Returns FALSE when the enemy is boxed in and could not move at all.
   The callers only run on the tick stateCounter hits 0, by which point the
   previous move has snapped exactly onto its target, so comparing position
   against moveTarget is a reliable "did this set a new move" test. */
static u16 enemyStepAwayOrSideways(const u16 id, enemy_t* enemy, const u8 targetX, const u8 targetY)
{
    enemyStepAwayFrom(id, enemy, targetX, targetY);

    if(enemy->x != enemy->moveTargetX || enemy->y != enemy->moveTargetY)
        return TRUE;

    enemyStepSideways(id, enemy, targetX, targetY);

    return (enemy->x != enemy->moveTargetX || enemy->y != enemy->moveTargetY);
}

static void enemySetTargetToPlayer(enemy_t* enemy)
{
    enemy->targetX = (u8)playerCellX;
    enemy->targetY = (u8)playerCellY;
}

void resetEnemy()
{
    u8 type;

    enemyCount = 0;
    viewTrigValid = FALSE;

    /* moveSpeed is a constant per type, so the tick count only needs deriving once. */
    for(type = 0; type < ENEMY_TYPE_COUNT; type++)
        enemyMoveTickTable[type] = fpMetersPerSecondToCellTicks(enemyStats[type].moveSpeed);
}

u16 getEnemyCell(u16 x, u16 y, s8 cell)
{
    u16 enemyId = enemyCount;
    u16 enemyType;

    if(enemyCount >= MAX_ENEMIES)
        return (MAP_MASK_SPRITE | MAP_MASK_WALK | SET_CELL_TYPE_ID(0) | 0);

    enemyCount++;

    if(enemyCount > MAX_ENEMIES)
        enemyCount = MAX_ENEMIES;

    switch(cell)
    {
        case 'E': //Mercenary
            enemyType = ENEMY_TYPE_MER;
            break;

        case 'F': //Soldier
            enemyType = ENEMY_TYPE_SGR;
            break;

        case 'G': //Heavy
            enemyType = ENEMY_TYPE_HVY;
            break;

        default: //Civilian
            enemyType = ENEMY_TYPE_CIV;
            break;
    }

    
    enemyList[enemyId].type = enemyType;

    //Start in idle state.
    enemyList[enemyId].state = ENEMY_STATE_IDLE;
    enemyList[enemyId].spriteFrame = ENEMY_FRAME_IDLE;
    enemyList[enemyId].spriteMirrored = FALSE;

    enemyList[enemyId].stateCounter = SECONDS_TO_TICKS(5);

    //Vary the start heading so a row of civilians does not set off in lockstep.
    enemyList[enemyId].wanderDir = (u8)(enemyId & 3);
    enemyList[enemyId].stateCells = 0;

    //Start at centre of cell.
    enemyList[enemyId].x = int2fp(x) + flt2fp(0.5f);
    enemyList[enemyId].y = int2fp(y) + flt2fp(0.5f);
    enemyList[enemyId].moveTargetX = enemyList[enemyId].x;
    enemyList[enemyId].moveTargetY = enemyList[enemyId].y;
    enemyList[enemyId].cellX = (u8)x;
    enemyList[enemyId].cellY = (u8)y;


    enemyList[enemyId].enemyStats = &enemyStats[enemyType];
    enemyList[enemyId].health = enemyStats[enemyType].health;

    //An enemy map character stands on bare floor.
    enemyList[enemyId].underCell = MAP_MASK_WALK;

    //No aim to remember yet.
    enemyList[enemyId].aimMemoryAge = ENEMY_AIM_MEMORY_TICKS;

    return enemyOverlayCell(enemyId, &enemyList[enemyId]);
}

enemy_t* getEnemy(u16 id)
{
    if(id < enemyCount)
    {
        return &enemyList[id];
    }

    return NULL;
}

u16 enemyBlocksPosition(f16 x, f16 y)
{
    u16 id;

    for(id = 0; id < enemyCount; id++)
    {
        const enemy_t* enemy = &enemyList[id];
        s16 dx;
        s16 dy;

        if(enemy->state == ENEMY_STATE_DEAD)
            continue;

        /* Map positions are at most MAP_X << FP_BITS, so the difference of two
           f16 always fits in 16 bits. Rejecting in 16 bit keeps the common case
           off the 32 bit helper path. */
        dx = x - enemy->x;
        dy = y - enemy->y;

        if(dx > ENEMY_COLLISION_RADIUS || dx < -ENEMY_COLLISION_RADIUS ||
            dy > ENEMY_COLLISION_RADIUS || dy < -ENEMY_COLLISION_RADIUS)
            continue;

        /* Only survivors of the box test need the widened distance check. */
        if((((s32)dx * dx) + ((s32)dy * dy)) <= ENEMY_COLLISION_RADIUS_SQ)
            return TRUE;
    }

    return FALSE;
}

void damageEnemy(u16 id, u8 damage)
{
    enemy_t* enemy = getEnemy(id);

    if(!enemy || enemy->state == ENEMY_STATE_DYING || enemy->state == ENEMY_STATE_DEAD)
        return;

    /* Called from shot resolution, so this lands at an arbitrary point in the
       enemy's stride rather than on a state boundary. Both interrupting paths
       below rewrite stateCounter, so the move has to be dropped first or the
       rest of the stride gets compressed into the new, much shorter counter. */
    if(damage >= enemy->health)
    {
        enemyCancelMove(enemy);
        enemy->health = 0;
        enemy->state = ENEMY_STATE_DYING;
        enemy->stateCounter = ENEMY_DYING_DELAY;
        enemy->spriteFrame = ENEMY_FRAME_DYING;
        return;
    }

    enemy->health -= damage;

    /* Below the stagger threshold the round lands but does not interrupt: the
       enemy keeps moving, keeps aiming and fires on schedule. */
    if(damage < enemy->enemyStats->staggerDamage)
        return;

    /* Already flinching: let it run out rather than restart it, or any weapon
       firing faster than the flinch holds the enemy in HURT indefinitely. */
    if(enemy->state == ENEMY_STATE_HURT)
        return;

    enemyCancelMove(enemy);

    /* Remember what was interrupted so that a flinch pauses an aim instead of
       resetting it. Anything other than an aim or attack resumes as a fresh
       chase, as it always did. */
    enemy->hurtResumeState = enemy->state;
    enemy->hurtResumeCounter = enemy->stateCounter;

    enemy->state = ENEMY_STATE_HURT;
    enemy->stateCounter = ENEMY_HURT_DELAY;
    enemy->spriteFrame = ENEMY_FRAME_HURT;
}

void runAI()
{
    u16 id;
    s16 enemyX;
    s16 enemyY;
    u16 dist;
    u16 canSee;

    playerCellX = fp2int(player.pos.x);
    playerCellY = fp2int(player.pos.y);

    for(id = 0; id < enemyCount; id++)
    {
        enemy_t* enemy = &enemyList[id];

        if(enemy->state == ENEMY_STATE_DYING)
        {
            enemy->spriteFrame = ENEMY_FRAME_DYING;

            if(enemy->stateCounter > 0)
                enemy->stateCounter--;
            else
            {
                enemy->state = ENEMY_STATE_DEAD;
                enemy->stateCounter = ENEMY_DROP_DELAY;
            }

            continue;
        }

        if(enemy->state == ENEMY_STATE_DEAD)
        {
            enemy->spriteFrame = ENEMY_FRAME_DEATH;

            /* Runs down once and stays at zero, so the release fires on the
               single tick the counter expires and never again. */
            if(enemy->stateCounter > 0)
            {
                enemy->stateCounter--;

                if(enemy->stateCounter == 0)
                    enemyReleaseCell(id, enemy);
            }

            continue;
        }

        dist = enemyDistanceToPlayer(enemy);

        if(dist > ENEMY_LEASH_DIST)
        {
            enemyCancelMove(enemy);
            enemy->state = ENEMY_STATE_IDLE;
            enemy->spriteFrame = ENEMY_FRAME_IDLE;
            enemy->stateCounter = 0;
            continue;
        }

        canSee = enemyCanSeePlayer(enemy);

        if(canSee)
            enemySetTargetToPlayer(enemy);

        /* A remembered aim goes stale while the enemy is doing anything other
           than aiming or firing. Saturates, so the compare is the whole cost. */
        if(enemy->state != ENEMY_STATE_AIMING && enemy->state != ENEMY_STATE_ATTACKING &&
           enemy->aimMemoryAge < ENEMY_AIM_MEMORY_TICKS)
            enemy->aimMemoryAge++;

        switch(enemy->state)
        {
            case ENEMY_STATE_IDLE:
                enemy->spriteFrame = ENEMY_FRAME_IDLE;

                if(enemyCounterTick(id, enemy))
                    break;

                //Civilians have no interest in the player, they just move on.
                if(enemy->type == ENEMY_TYPE_CIV)
                {
                    enemy->state = ENEMY_STATE_WANDER;
                    enemy->stateCounter = 0;
                    break;
                }

                if(!canSee)
                {
                    enemy->state = ENEMY_STATE_IDLE;
                    enemy->stateCounter = ENEMY_IDLE_DELAY;
                    break;
                }

                if(enemyRandomBit())
                {
                    enemy->state = ENEMY_STATE_CHASING;
                    enemy->stateCounter = 0;
                    break;
                }

                enemy->state = ENEMY_STATE_SURPRISED;
                enemy->stateCounter = ENEMY_SURPRISED_DELAY;
                break;

            case ENEMY_STATE_SEARCHING:
                /* Tested before the counter, not after it. The counter is the
                   pause before the first search step - a full move period, four
                   seconds for a Heavy - and testing sight only on its expiry
                   made that a blind window: duck for a moment, step back out,
                   and anything that could not stagger the enemy had four free
                   seconds against a target that would not look up. */
                if(canSee)
                {
                    enemy->state = ENEMY_STATE_CHASING;
                    enemy->stateCounter = 0;
                    break;
                }

                if(enemyCounterTick(id, enemy))
                    break;

                /* Out of patience. Without this a search never ends: an enemy
                   that cannot reach the target cell never satisfies the arrival
                   test and hunts for it forever. */
                if(enemy->stateCells == 0)
                {
                    enemy->state = ENEMY_STATE_IDLE;
                    enemy->spriteFrame = ENEMY_FRAME_IDLE;
                    enemy->stateCounter = ENEMY_IDLE_DELAY;
                    break;
                }

                enemy->stateCells--;

                enemyX = fp2int(enemy->x);
                enemyY = fp2int(enemy->y);

                /* Adjacent counts as arrived. Alerts send several enemies to
                   one cell and only one of them can stand on it, so demanding
                   an exact match left the rest grinding against the occupant. */
                if((absDiff(enemyX, enemy->targetX) + absDiff(enemyY, enemy->targetY)) <= 1)
                {
                    //Nothing here, so look around instead of giving up on the spot.
                    enemyWanderStep(id, enemy);
                    enemySetMoveCounter(enemy, enemyMoveTicks(enemy));
                    break;
                }

                /* A failed step just costs this period. Transient blockers are
                   common now that alerts move groups around, and the patience
                   budget already handles the case that never clears. */
                enemyStepToward(id, enemy, enemy->targetX, enemy->targetY);
                enemySetMoveCounter(enemy, enemyMoveTicks(enemy));
                break;

            case ENEMY_STATE_SURPRISED:
                enemy->spriteFrame = ENEMY_FRAME_IDLE;

                if(enemyCounterTick(id, enemy))
                    break;

                enemy->state = ENEMY_STATE_CHASING;
                enemy->stateCounter = 0;
                break;

            case ENEMY_STATE_CHASING:
                if(enemyCounterTick(id, enemy))
                    break;

                /* Civilians must never chase - with no attack state they walk
                   onto the player and pin them there. Every route into this
                   case goes through enemyAlertState, so this only catches a
                   future one that does not. */
                if(enemy->type == ENEMY_TYPE_CIV)
                {
                    enemy->state = ENEMY_STATE_WANDER;
                    enemy->stateCounter = 0;
                    break;
                }

                if(!canSee)
                {
                    enemyStartSearch(enemy, enemyMoveTicks(enemy));
                    break;
                }

                /* Evading is a reaction to being hit and is rolled on the way
                   out of HURT. It used to be rolled here too, on every chase
                   step, so enemies jinked at random on the approach and a Heavy
                   could stall for four seconds having taken no fire at all.

                   The target is already the player's cell: it is refreshed at
                   the top of every tick that canSee, and canSee held to get here. */
                if(dist <= enemyAttackDistance(enemy))
                {
                    /* Too close to shoot comfortably, so give ground - but only
                       if there is ground to give. Cornered, the retreat fails
                       every move period and the enemy used to sit there retrying
                       it forever, never reaching the aim below. Backed into a
                       dead end it fights instead. */
                    if(dist < enemyAttackMinDistance(enemy) &&
                       enemyStepAwayOrSideways(id, enemy, enemy->targetX, enemy->targetY))
                    {
                        enemy->state = ENEMY_STATE_CHASING;
                        enemySetMoveCounter(enemy, enemyMoveTicks(enemy));
                        break;
                    }

                    enemyStartAim(enemy);
                    break;
                }

                enemyStepToward(id, enemy, enemy->targetX, enemy->targetY);
                enemySetMoveCounter(enemy, enemyMoveTicks(enemy));
                break;

            case ENEMY_STATE_AIMING:
                enemy->spriteFrame = ENEMY_FRAME_AIM;

                /* Checked every tick rather than only when the aim completes,
                   so a target that steps behind cover is broken off at once
                   instead of drawing a half-second aim at a wall. Nothing is in
                   flight during an aim, so skipping the counter tick loses no
                   movement. The aim owed so far is kept, see enemyLoseAim. */
                if(!canSee)
                {
                    enemyLoseAim(enemy, enemy->stateCounter);
                    break;
                }

                if(enemyCounterTick(id, enemy))
                    break;

                enemy->state = ENEMY_STATE_ATTACKING;
                enemy->stateCounter = enemy->enemyStats->attackTicks;
                enemyShootPlayer(id, enemy);
                break;

            case ENEMY_STATE_WANDER:
                if(enemyCounterTick(id, enemy))
                    break;

                //Keep out of the player's way before picking anywhere to go.
                if(canSee && dist <= ENEMY_CIV_AVOID_DIST)
                {
                    enemyStepAwayOrSideways(id, enemy, (u8)playerCellX, (u8)playerCellY);
                    enemySetMoveCounter(enemy, enemyMoveTicks(enemy));
                    break;
                }

                if(enemyRandomChance(ENEMY_WANDER_PAUSE_CHANCE))
                {
                    enemy->state = ENEMY_STATE_IDLE;
                    enemy->spriteFrame = ENEMY_FRAME_IDLE;
                    enemy->stateCounter = ENEMY_IDLE_DELAY;
                    break;
                }

                enemyWanderStep(id, enemy);
                enemySetMoveCounter(enemy, enemyMoveTicks(enemy));
                break;

            case ENEMY_STATE_FLEEING:
                if(enemyCounterTick(id, enemy))
                    break;

                if(enemy->stateCells > 0)
                    enemy->stateCells--;

                //Run the guaranteed distance first, then until out of danger.
                if(enemy->stateCells == 0 && (!canSee || dist > ENEMY_FLEE_SAFE_DIST))
                {
                    enemy->state = enemyAlertState(enemy);
                    enemy->stateCounter = 0;
                    break;
                }

                //Nowhere left to run, so stop running.
                if(!enemyStepAwayOrSideways(id, enemy, (u8)playerCellX, (u8)playerCellY))
                {
                    enemy->state = enemyAlertState(enemy);
                    enemy->stateCounter = 0;
                    break;
                }

                enemySetMoveCounter(enemy, enemyMoveTicks(enemy));
                break;

            case ENEMY_STATE_EVADING:
                if(enemyCounterTick(id, enemy))
                    break;

                enemyStepSideways(id, enemy, (u8)playerCellX, (u8)playerCellY);
                enemy->state = enemyAlertState(enemy);
                enemySetMoveCounter(enemy, ENEMY_SIDESTEP_TICKS);
                break;

            case ENEMY_STATE_HURT:
                enemy->spriteFrame = ENEMY_FRAME_HURT;

                if(enemyCounterTick(id, enemy))
                    break;

                /* Shot while already running: keep running. Rolling flee or
                   evade here would either restart the flight or, worse, sidestep
                   back into the fight. stateCells still holds the distance left,
                   and the counter is zeroed because the flinch cancelled the
                   step that was in flight. */
                if(enemy->hurtResumeState == ENEMY_STATE_FLEEING)
                {
                    enemy->state = ENEMY_STATE_FLEEING;
                    enemy->stateCounter = 0;
                    break;
                }

                if(enemyRandomChance(enemy->enemyStats->fleeChance))
                {
                    enemyStartFlee(enemy);
                    break;
                }

                if(enemyRandomChance(enemy->enemyStats->evadeChance))
                {
                    enemy->state = ENEMY_STATE_EVADING;
                    enemy->stateCounter = ENEMY_EVADE_DELAY;
                    break;
                }

                /* Pick the aim back up where it was interrupted. Under sustained
                   fire the enemy still gains ground toward its shot between
                   flinches instead of restarting from zero every time. */
                if(enemy->hurtResumeState == ENEMY_STATE_AIMING ||
                   enemy->hurtResumeState == ENEMY_STATE_ATTACKING)
                {
                    enemy->state = enemy->hurtResumeState;
                    enemy->stateCounter = enemy->hurtResumeCounter;
                    break;
                }

                enemy->state = enemyAlertState(enemy);
                enemy->stateCounter = 0;
                break;

            case ENEMY_STATE_ATTACKING:
                /* The counter runs down from attackTicks after each round, so
                   the flash is the top few ticks of it. An interval shorter than
                   the flash simply stays lit, as a fast gun should. */
                enemy->spriteFrame =
                    (enemy->stateCounter + ENEMY_FLASH_TICKS > enemy->enemyStats->attackTicks)
                        ? ENEMY_FRAME_SHOOT : ENEMY_FRAME_AIM;

                if(enemyCounterTick(id, enemy))
                    break;

                /* Shots come in bursts. The aim is the wind-up before a burst;
                   attackTicks is the interval between rounds within one; and
                   each round rolls repositionChance to decide whether it was the
                   last, so that one knob sets burst length and movement together.
                   A burst only carries on at something still in sight and still
                   in reach - otherwise it is a search or a chase. Losing sight
                   mid-burst leaves no aim owed: reappear soon and the burst
                   resumes after the reacquire floor. */
                if(!canSee)
                {
                    enemyLoseAim(enemy, 0);
                    break;
                }

                if(dist > enemyAttackDistance(enemy))
                {
                    enemy->state = ENEMY_STATE_CHASING;
                    enemy->stateCounter = 0;
                    break;
                }

                /* Burst over: shift position, then wind up again. The step is a
                   short fixed sidestep, so it trades fire rate for being a
                   harder target - a Merc's habit, not a Heavy's. */
                if(enemyRandomChance(enemy->enemyStats->repositionChance))
                {
                    enemyStepSideways(id, enemy, (u8)playerCellX, (u8)playerCellY);
                    enemy->state = ENEMY_STATE_CHASING;
                    enemySetMoveCounter(enemy, ENEMY_SIDESTEP_TICKS);
                    break;
                }

                //Next round of the burst, no re-aim.
                enemy->stateCounter = enemy->enemyStats->attackTicks;
                enemyShootPlayer(id, enemy);
                break;
        }
    }
}
