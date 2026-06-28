#include "enemy.h"
#include "game_map.h"

enemy_t enemyList[MAX_ENEMIES];

u16 enemyCount = 0;

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
    //TODO
}