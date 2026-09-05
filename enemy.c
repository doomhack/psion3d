#include "enemy.h"
#include "sprslot.h"
#include "game_map.h"
#include "psion3d.h"
#include "units.h"

#define ENEMY_LEASH_DIST_METERS 28
#define ENEMY_ATTACK_DIST_MER_METERS 4
#define ENEMY_ATTACK_DIST_SGR_METERS 6
#define ENEMY_ATTACK_DIST_HVY_METERS 8
#define ENEMY_ATTACK_MIN_DIST_METERS 4
#define ENEMY_CIV_AVOID_DIST_METERS 6
#define ENEMY_FLEE_SAFE_DIST_METERS 12
#define ENEMY_ALERT_DIST_METERS 20

#define ENEMY_LEASH_DIST METERS_TO_MAP_CELLS(ENEMY_LEASH_DIST_METERS)
#define ENEMY_ATTACK_DIST_MER METERS_TO_MAP_CELLS(ENEMY_ATTACK_DIST_MER_METERS)
#define ENEMY_ATTACK_DIST_SGR METERS_TO_MAP_CELLS(ENEMY_ATTACK_DIST_SGR_METERS)
#define ENEMY_ATTACK_DIST_HVY METERS_TO_MAP_CELLS(ENEMY_ATTACK_DIST_HVY_METERS)
#define ENEMY_ATTACK_MIN_DIST METERS_TO_MAP_CELLS(ENEMY_ATTACK_MIN_DIST_METERS)
#define ENEMY_CIV_AVOID_DIST METERS_TO_MAP_CELLS(ENEMY_CIV_AVOID_DIST_METERS)
#define ENEMY_FLEE_SAFE_DIST METERS_TO_MAP_CELLS(ENEMY_FLEE_SAFE_DIST_METERS)
#define ENEMY_ALERT_DIST METERS_TO_MAP_CELLS(ENEMY_ALERT_DIST_METERS)

#define ENEMY_ATTACK_DELAY SECONDS_TO_TICKS(1)
#define ENEMY_IDLE_DELAY SECONDS_TO_TICKS(1)
#define ENEMY_SURPRISED_DELAY fpSecondsToTicks(flt2fp(0.5f))
#define ENEMY_AIM_DELAY fpSecondsToTicks(flt2fp(0.5f))
#define ENEMY_EVADE_DELAY fpSecondsToTicks(flt2fp(0.4f))
#define ENEMY_HURT_DELAY fpSecondsToTicks(flt2fp(0.25f))
#define ENEMY_DYING_DELAY fpSecondsToTicks(flt2fp(0.5f))

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
	{flt2fp(1),    48, 240, 100,  0,  0,  SPRITE_SLOT_CIV}, //Civilian
	{flt2fp(1.5),    32, 16,  75,   10, 10, SPRITE_SLOT_MER}, //Mercenary
	{flt2fp(2),  64, 8,   100,  15, 40, SPRITE_SLOT_SGR}, //Soldier
	{flt2fp(0.5),  16, 4,   250,  25, 25, SPRITE_SLOT_HVY}  //Heavy
};


#define ENEMY_TYPE_COUNT 4

/* Derived once per map load from enemyStats[].moveSpeed. */
static u8 enemyMoveTickTable[ENEMY_TYPE_COUNT];

enemy_t enemyList[MAX_ENEMIES];

u16 enemyCount = 0;

static u16 enemyRand = 0x9a31;

static u16 absDiff(const s16 a, const s16 b)
{
    return a > b ? a - b : b - a;
}

static u16 enemyCellValue(const u16 id, const enemy_t* enemy)
{
    return (MAP_MASK_SPRITE | MAP_MASK_ENEMY | MAP_MASK_WALK | SET_CELL_TYPE_ID(enemy->type) | id);
}

static u16 enemyDistanceToPlayer(const enemy_t* enemy)
{
    s16 enemyX = fp2int(enemy->x);
    s16 enemyY = fp2int(enemy->y);
    s16 playerX = fp2int(player.pos.x);
    s16 playerY = fp2int(player.pos.y);

    return absDiff(enemyX, playerX) + absDiff(enemyY, playerY);
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

static void enemyUpdateMapCell(const u16 id, enemy_t* enemy)
{
    u8 newX = (u8)fp2int(enemy->x);
    u8 newY = (u8)fp2int(enemy->y);
    u16 oldCell;

    if(newX == enemy->cellX && newY == enemy->cellY)
        return;

    oldCell = mapCell(enemy->cellX, enemy->cellY);

    if(isEnemy(oldCell) && GET_CELL_ID(oldCell) == id)
        updateCell(enemy->cellX, enemy->cellY, MAP_MASK_WALK);

    updateCell(newX, newY, enemyCellValue(id, enemy));

    enemy->cellX = newX;
    enemy->cellY = newY;
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

static void enemyFinishMove(const u16 id, enemy_t* enemy)
{
    enemy->x = enemy->moveTargetX;
    enemy->y = enemy->moveTargetY;
    enemyUpdateMapCell(id, enemy);
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

        /* Abandons any move in flight. The next step interpolates from wherever
           the enemy stopped, so this reads as a reaction rather than a jump. */
        enemy->stateCounter = 0;

        //Civilians run from trouble. Everyone else goes to find it.
        if(enemy->type == ENEMY_TYPE_CIV)
        {
            enemy->state = ENEMY_STATE_FLEEING;
            enemy->stateCells = ENEMY_FLEE_CELLS;
        }
        else
        {
            enemy->state = ENEMY_STATE_SEARCHING;
            enemy->stateCells = ENEMY_SEARCH_CELLS;
        }
    }
}

static void enemyShootPlayer(const enemy_t* enemy)
{
    u8 damage;

    /* A shot is loud whether or not it connects, and this is what carries the
       alarm outward - the player's shot wakes the first room, those enemies
       firing back wake the next one. */
    alertEnemies((u8)fp2int(enemy->x), (u8)fp2int(enemy->y));

    if(!enemyRandomChance(enemy->enemyStats->accuracy) || player.health == 0)
        return;

    damage = enemy->enemyStats->damage;

    if(damage >= player.health)
        player.health = 0;
    else
        player.health -= damage;
}

static u16 enemyCanSeePlayer(const enemy_t* enemy)
{
    s16 x0 = fp2int(enemy->x);
    s16 y0 = fp2int(enemy->y);
    s16 x1 = fp2int(player.pos.x);
    s16 y1 = fp2int(player.pos.y);
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

        if(isWall(mapCell(x0, y0)))
            return FALSE;
    }

    return TRUE;
}

static u16 enemyTryMoveTo(const u16 id, enemy_t* enemy, const s16 newX, const s16 newY)
{
    if(newX < 0 || newY < 0 || newX >= MAP_X || newY >= MAP_Y)
        return FALSE;

    if(mapCell(newX, newY) != MAP_MASK_WALK)
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
    enemy->targetX = (u8)fp2int(player.pos.x);
    enemy->targetY = (u8)fp2int(player.pos.y);
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

    return (MAP_MASK_SPRITE | MAP_MASK_ENEMY | MAP_MASK_WALK | SET_CELL_TYPE_ID(enemyType) | enemyId);
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

    if(damage >= enemy->health)
    {
        enemy->health = 0;
        enemy->state = ENEMY_STATE_DYING;
        enemy->stateCounter = ENEMY_DYING_DELAY;
        enemy->spriteFrame = ENEMY_FRAME_DYING;
        return;
    }

    enemy->health -= damage;
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

    for(id = 0; id < enemyCount; id++)
    {
        enemy_t* enemy = &enemyList[id];

        if(enemy->state == ENEMY_STATE_DYING)
        {
            enemy->spriteFrame = ENEMY_FRAME_DYING;

            if(enemy->stateCounter > 0)
                enemy->stateCounter--;
            else
                enemy->state = ENEMY_STATE_DEAD;

            continue;
        }

        if(enemy->state == ENEMY_STATE_DEAD)
        {
            enemy->spriteFrame = ENEMY_FRAME_DEATH;
            continue;
        }

        dist = enemyDistanceToPlayer(enemy);

        if(dist > ENEMY_LEASH_DIST)
        {
            enemyFinishMove(id, enemy);
            enemy->state = ENEMY_STATE_IDLE;
            enemy->spriteFrame = ENEMY_FRAME_IDLE;
            enemy->stateCounter = 0;
            continue;
        }

        canSee = enemyCanSeePlayer(enemy);

        if(canSee)
            enemySetTargetToPlayer(enemy);

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
                if(enemyCounterTick(id, enemy))
                    break;

                if(canSee)
                {
                    enemy->state = ENEMY_STATE_CHASING;
                    enemy->stateCounter = 0;
                    break;
                }

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
                    enemy->state = ENEMY_STATE_SEARCHING;
                    enemy->stateCells = ENEMY_SEARCH_CELLS;
                    enemy->stateCounter = enemyMoveTicks(enemy);
                    break;
                }

                if(enemyRandomChance(enemy->enemyStats->evadeChance))
                {
                    enemy->state = ENEMY_STATE_EVADING;
                    enemy->stateCounter = ENEMY_EVADE_DELAY;
                    break;
                }

                enemySetTargetToPlayer(enemy);

                if(enemy->type != ENEMY_TYPE_CIV && dist <= enemyAttackDistance(enemy))
                {
                    /* Too close to shoot comfortably, so give ground - but only
                       if there is ground to give. Cornered, the retreat fails
                       every move period and the enemy used to sit there retrying
                       it forever, never reaching the aim below. Backed into a
                       dead end it fights instead. */
                    if(dist < ENEMY_ATTACK_MIN_DIST &&
                       enemyStepAwayOrSideways(id, enemy, enemy->targetX, enemy->targetY))
                    {
                        enemy->state = ENEMY_STATE_CHASING;
                        enemySetMoveCounter(enemy, enemyMoveTicks(enemy));
                        break;
                    }

                    enemy->state = ENEMY_STATE_AIMING;
                    enemy->stateCounter = ENEMY_AIM_DELAY;
                    enemy->spriteFrame = ENEMY_FRAME_AIM;
                    break;
                }

                enemyStepToward(id, enemy, enemy->targetX, enemy->targetY);
                enemySetMoveCounter(enemy, enemyMoveTicks(enemy));
                break;

            case ENEMY_STATE_AIMING:
                enemy->spriteFrame = ENEMY_FRAME_AIM;

                if(enemyCounterTick(id, enemy))
                    break;

                if(!canSee)
                {
                    enemy->state = ENEMY_STATE_SEARCHING;
                    enemy->stateCells = ENEMY_SEARCH_CELLS;
                    enemy->stateCounter = enemyMoveTicks(enemy);
                    break;
                }

                enemy->state = ENEMY_STATE_ATTACKING;
                enemy->stateCounter = ENEMY_ATTACK_DELAY;
                enemyShootPlayer(enemy);
                break;

            case ENEMY_STATE_WANDER:
                if(enemyCounterTick(id, enemy))
                    break;

                //Keep out of the player's way before picking anywhere to go.
                if(canSee && dist <= ENEMY_CIV_AVOID_DIST)
                {
                    enemyStepAwayOrSideways(id, enemy, (u8)fp2int(player.pos.x), (u8)fp2int(player.pos.y));
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
                if(!enemyStepAwayOrSideways(id, enemy, (u8)fp2int(player.pos.x), (u8)fp2int(player.pos.y)))
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

                enemyStepSideways(id, enemy, (u8)fp2int(player.pos.x), (u8)fp2int(player.pos.y));
                enemy->state = enemyAlertState(enemy);
                enemySetMoveCounter(enemy, enemyMoveTicks(enemy));
                break;

            case ENEMY_STATE_HURT:
                enemy->spriteFrame = ENEMY_FRAME_HURT;

                if(enemyCounterTick(id, enemy))
                    break;

                if(enemyRandomChance(enemy->enemyStats->fleeChance))
                {
                    enemy->state = ENEMY_STATE_FLEEING;
                    enemy->stateCells = ENEMY_FLEE_CELLS;
                    enemy->stateCounter = 0;
                    break;
                }

                if(enemyRandomChance(enemy->enemyStats->evadeChance))
                {
                    enemy->state = ENEMY_STATE_EVADING;
                    enemy->stateCounter = ENEMY_EVADE_DELAY;
                    break;
                }

                enemy->state = enemyAlertState(enemy);
                enemy->stateCounter = 0;
                break;

            case ENEMY_STATE_ATTACKING:
                enemy->spriteFrame = ENEMY_FRAME_SHOOT;

                if(enemyCounterTick(id, enemy))
                    break;

                if(enemyRandomBit())
                {
                    enemy->state = ENEMY_STATE_AIMING;
                    enemy->stateCounter = ENEMY_AIM_DELAY;
                    enemy->spriteFrame = ENEMY_FRAME_AIM;
                    break;
                }

                enemy->state = ENEMY_STATE_CHASING;
                enemy->stateCounter = 0;
                break;
        }
    }
}
