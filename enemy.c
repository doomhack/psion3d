#include "enemy.h"
#include "game_map.h"
#include "psion3d.h"

#define ENEMY_LEASH_DIST 14
#define ENEMY_ATTACK_DIST_MER 2
#define ENEMY_ATTACK_DIST_SGR 3
#define ENEMY_ATTACK_DIST_HVY 4
#define ENEMY_ATTACK_MIN_DIST 2
#define ENEMY_MOVE_DELAY 8
#define ENEMY_ATTACK_DELAY 20
#define ENEMY_IDLE_DELAY 20
#define ENEMY_SURPRISED_DELAY 10
#define ENEMY_AIM_DELAY 10
#define ENEMY_EVADE_DELAY 8

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
    s16 playerX = fp2int(pos.x);
    s16 playerY = fp2int(pos.y);

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

static u8 enemyRandomBit()
{
    enemyRand = (u16)(enemyRand * 17 + 43);

    return (u8)((enemyRand >> 8) & 1);
}

static u16 enemyCanSeePlayer(const enemy_t* enemy)
{
    s16 x0 = fp2int(enemy->x);
    s16 y0 = fp2int(enemy->y);
    s16 x1 = fp2int(pos.x);
    s16 y1 = fp2int(pos.y);
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
    s16 oldX = fp2int(enemy->x);
    s16 oldY = fp2int(enemy->y);
    u16 cell;

    if(newX < 0 || newY < 0 || newX >= MAP_X || newY >= MAP_Y)
        return FALSE;

    if(mapCell(newX, newY) != MAP_MASK_WALK)
        return FALSE;

    cell = mapCell(oldX, oldY);

    if(!isEnemy(cell) || GET_CELL_ID(cell) != id)
        cell = enemyCellValue(id, enemy);

    updateCell(oldX, oldY, MAP_MASK_WALK);
    updateCell(newX, newY, cell & ~MAP_MASK_MARKED);

    enemy->x = int2fp(newX) + flt2fp(0.5f);
    enemy->y = int2fp(newY) + flt2fp(0.5f);

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
    enemy->targetX = (u8)fp2int(pos.x);
    enemy->targetY = (u8)fp2int(pos.y);
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

    enemyList[enemyId].stateCounter = 100;

    //Start at centre of cell.
    enemyList[enemyId].x = int2fp(x) + flt2fp(0.5f);
    enemyList[enemyId].y = int2fp(y) + flt2fp(0.5f);

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
            enemy->state = ENEMY_STATE_IDLE;
            enemy->spriteFrame = ENEMY_FRAME_IDLE;
            continue;
        }

        canSee = enemyCanSeePlayer(enemy);

        if(canSee)
            enemySetTargetToPlayer(enemy);

        switch(enemy->state)
        {
            case ENEMY_STATE_IDLE:
                enemy->spriteFrame = ENEMY_FRAME_IDLE;

                if(enemy->stateCounter > 0)
                {
                    enemy->stateCounter--;
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
                if(enemy->stateCounter > 0)
                {
                    enemy->stateCounter--;
                    break;
                }

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
                enemy->spriteFrame = ENEMY_FRAME_WALK_R1;
                enemy->stateCounter = ENEMY_MOVE_DELAY;
                break;

            case ENEMY_STATE_SURPRISED:
                enemy->spriteFrame = ENEMY_FRAME_IDLE;

                if(enemy->stateCounter > 0)
                {
                    enemy->stateCounter--;
                    break;
                }

                enemy->state = ENEMY_STATE_CHASING;
                enemy->stateCounter = 0;
                break;

            case ENEMY_STATE_CHASING:
                if(enemy->stateCounter > 0)
                {
                    enemy->stateCounter--;
                    break;
                }

                if(!canSee)
                {
                    enemy->state = ENEMY_STATE_SEARCHING;
                    enemy->stateCounter = ENEMY_MOVE_DELAY;
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
                        enemy->spriteFrame = ENEMY_FRAME_WALK_R1;
                        enemy->state = ENEMY_STATE_CHASING;
                        enemy->stateCounter = ENEMY_MOVE_DELAY;
                        break;
                    }

                    enemy->state = ENEMY_STATE_AIMING;
                    enemy->stateCounter = ENEMY_AIM_DELAY;
                    enemy->spriteFrame = ENEMY_FRAME_AIM;
                    break;
                }

                enemyStepToward(id, enemy, enemy->targetX, enemy->targetY);
                enemy->spriteFrame = ENEMY_FRAME_WALK_R1;
                enemy->stateCounter = ENEMY_MOVE_DELAY;
                break;

            case ENEMY_STATE_AIMING:
                enemy->spriteFrame = ENEMY_FRAME_AIM;

                if(enemy->stateCounter > 0)
                {
                    enemy->stateCounter--;
                    break;
                }

                if(!canSee)
                {
                    enemy->state = ENEMY_STATE_SEARCHING;
                    enemy->stateCounter = ENEMY_MOVE_DELAY;
                    break;
                }

                enemy->state = ENEMY_STATE_ATTACKING;
                enemy->stateCounter = ENEMY_ATTACK_DELAY;
                break;

            case ENEMY_STATE_EVADING:
                enemy->spriteFrame = ENEMY_FRAME_WALK_R1;

                if(enemy->stateCounter > 0)
                {
                    enemy->stateCounter--;
                    break;
                }

                enemyStepSideways(id, enemy, (u8)fp2int(pos.x), (u8)fp2int(pos.y));
                enemy->state = ENEMY_STATE_CHASING;
                enemy->stateCounter = 0;
                break;

            case ENEMY_STATE_HURT:
                enemy->spriteFrame = ENEMY_FRAME_HURT;

                if(enemy->stateCounter > 0)
                {
                    enemy->stateCounter--;
                    break;
                }

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

                if(enemy->stateCounter > 0)
                {
                    enemy->stateCounter--;
                    break;
                }

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
