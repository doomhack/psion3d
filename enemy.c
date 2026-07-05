#include "enemy.h"
#include "game_map.h"
#include "psion3d.h"
#include "units.h"

#define ENEMY_LEASH_DIST_METERS 28
#define ENEMY_ATTACK_DIST_MER_METERS 4
#define ENEMY_ATTACK_DIST_SGR_METERS 6
#define ENEMY_ATTACK_DIST_HVY_METERS 8
#define ENEMY_ATTACK_MIN_DIST_METERS 4

#define ENEMY_LEASH_DIST METERS_TO_MAP_CELLS(ENEMY_LEASH_DIST_METERS)
#define ENEMY_ATTACK_DIST_MER METERS_TO_MAP_CELLS(ENEMY_ATTACK_DIST_MER_METERS)
#define ENEMY_ATTACK_DIST_SGR METERS_TO_MAP_CELLS(ENEMY_ATTACK_DIST_SGR_METERS)
#define ENEMY_ATTACK_DIST_HVY METERS_TO_MAP_CELLS(ENEMY_ATTACK_DIST_HVY_METERS)
#define ENEMY_ATTACK_MIN_DIST METERS_TO_MAP_CELLS(ENEMY_ATTACK_MIN_DIST_METERS)

#define ENEMY_MOVE_SPEED_MPS flt2fp(4.5f)
#define ENEMY_MOVE_TICKS fpMetersPerSecondToCellTicks(ENEMY_MOVE_SPEED_MPS)
#define ENEMY_ATTACK_DELAY SECONDS_TO_TICKS(1)
#define ENEMY_IDLE_DELAY SECONDS_TO_TICKS(1)
#define ENEMY_SURPRISED_DELAY fpSecondsToTicks(flt2fp(0.5f))
#define ENEMY_AIM_DELAY fpSecondsToTicks(flt2fp(0.5f))
#define ENEMY_EVADE_DELAY fpSecondsToTicks(flt2fp(0.4f))

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

static void enemySetWalkFrame(enemy_t* enemy)
{
    f16 f_dx = enemy->moveTargetX - enemy->x;
    f16 f_dy = enemy->moveTargetY - enemy->y;
    f16 f_side =
        -fpmul(f_dx, fpsin(player.pos.angle)) +
         fpmul(f_dy, fpcos(player.pos.angle));

    if(f_side > 0)
        enemy->spriteFrame = (enemy->stateCounter & 1) ? ENEMY_FRAME_WALK_R1 : ENEMY_FRAME_WALK_R2;
    else
        enemy->spriteFrame = (enemy->stateCounter & 1) ? ENEMY_FRAME_WALK_L1 : ENEMY_FRAME_WALK_L2;
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

    enemy->x += fpdiv(enemy->moveTargetX - enemy->x, int2fp(enemy->stateCounter));
    enemy->y += fpdiv(enemy->moveTargetY - enemy->y, int2fp(enemy->stateCounter));
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

static void enemyStepToward(const u16 id, enemy_t* enemy, const u8 targetX, const u8 targetY)
{
    s16 enemyX = fp2int(enemy->x);
    s16 enemyY = fp2int(enemy->y);
    s16 dx = targetX - enemyX;
    s16 dy = targetY - enemyY;
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

static void enemySetTargetToPlayer(enemy_t* enemy)
{
    enemy->targetX = (u8)fp2int(player.pos.x);
    enemy->targetY = (u8)fp2int(player.pos.y);
}

void resetEnemy()
{
    enemyCount = 0;
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

    enemyList[enemyId].stateCounter = SECONDS_TO_TICKS(5);

    //Start at centre of cell.
    enemyList[enemyId].x = int2fp(x) + flt2fp(0.5f);
    enemyList[enemyId].y = int2fp(y) + flt2fp(0.5f);
    enemyList[enemyId].moveTargetX = enemyList[enemyId].x;
    enemyList[enemyId].moveTargetY = enemyList[enemyId].y;
    enemyList[enemyId].cellX = (u8)x;
    enemyList[enemyId].cellY = (u8)y;

    switch(enemyType)
    {
        case ENEMY_TYPE_CIV:
            enemyList[enemyId].health = 50;
            enemyList[enemyId].spriteId = SPRITE_SLOT_CIV;
            break;

        case ENEMY_TYPE_MER:
            enemyList[enemyId].health = 50;
            enemyList[enemyId].spriteId = SPRITE_SLOT_MER;
            break;

        case ENEMY_TYPE_SGR:
            enemyList[enemyId].health = 75;
            enemyList[enemyId].spriteId = SPRITE_SLOT_SGR;
            break;

        case ENEMY_TYPE_HVY:
            enemyList[enemyId].health = 100;
            enemyList[enemyId].spriteId = SPRITE_SLOT_HVY;
            break;

    }

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

                enemyX = fp2int(enemy->x);
                enemyY = fp2int(enemy->y);

                if(enemyX == enemy->targetX && enemyY == enemy->targetY)
                {
                    enemy->state = ENEMY_STATE_IDLE;
                    enemy->spriteFrame = ENEMY_FRAME_IDLE;
                    break;
                }

                enemyStepToward(id, enemy, enemy->targetX, enemy->targetY);
                enemySetMoveCounter(enemy, ENEMY_MOVE_TICKS);
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

                if(!canSee)
                {
                    enemy->state = ENEMY_STATE_SEARCHING;
                    enemy->stateCounter = ENEMY_MOVE_TICKS;
                    break;
                }

                if(enemyRandomBit())
                {
                    enemy->state = ENEMY_STATE_EVADING;
                    enemy->stateCounter = ENEMY_EVADE_DELAY;
                    break;
                }

                enemySetTargetToPlayer(enemy);

                if(enemy->type != ENEMY_TYPE_CIV && dist <= enemyAttackDistance(enemy))
                {
                    if(dist < ENEMY_ATTACK_MIN_DIST)
                    {
                        enemyStepAwayFrom(id, enemy, enemy->targetX, enemy->targetY);
                        enemy->state = ENEMY_STATE_CHASING;
                        enemySetMoveCounter(enemy, ENEMY_MOVE_TICKS);
                        break;
                    }

                    enemy->state = ENEMY_STATE_AIMING;
                    enemy->stateCounter = ENEMY_AIM_DELAY;
                    enemy->spriteFrame = ENEMY_FRAME_AIM;
                    break;
                }

                enemyStepToward(id, enemy, enemy->targetX, enemy->targetY);
                enemySetMoveCounter(enemy, ENEMY_MOVE_TICKS);
                break;

            case ENEMY_STATE_AIMING:
                enemy->spriteFrame = ENEMY_FRAME_AIM;

                if(enemyCounterTick(id, enemy))
                    break;

                if(!canSee)
                {
                    enemy->state = ENEMY_STATE_SEARCHING;
                    enemy->stateCounter = ENEMY_MOVE_TICKS;
                    break;
                }

                enemy->state = ENEMY_STATE_ATTACKING;
                enemy->stateCounter = ENEMY_ATTACK_DELAY;
                break;

            case ENEMY_STATE_EVADING:
                if(enemyCounterTick(id, enemy))
                    break;

                enemyStepSideways(id, enemy, (u8)fp2int(player.pos.x), (u8)fp2int(player.pos.y));
                enemy->state = ENEMY_STATE_CHASING;
                enemySetMoveCounter(enemy, ENEMY_MOVE_TICKS);
                break;

            case ENEMY_STATE_HURT:
                enemy->spriteFrame = ENEMY_FRAME_HURT;

                if(enemyCounterTick(id, enemy))
                    break;

                if(enemyRandomBit())
                {
                    enemy->state = ENEMY_STATE_EVADING;
                    enemy->stateCounter = ENEMY_EVADE_DELAY;
                    break;
                }

                enemy->state = ENEMY_STATE_CHASING;
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
