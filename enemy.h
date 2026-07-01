#ifndef ENEMY_H
#define ENEMY_H

#include "fp_types.h"

//Enemy is idle state. 
#define ENEMY_STATE_IDLE 0

//Enemy has been awaken and is searching for player.
#define ENEMY_STATE_SEARCHING 1

//Enemy has seen player. TargetX/Y is the location the player was in when last seen.
#define ENEMY_STATE_CHASING 2

//Enemy has line of sight to player and attacking.
#define ENEMY_STATE_ATTACKING 3

//Enemy has just seen the player and pauses before reacting.
#define ENEMY_STATE_SURPRISED 4

//Enemy is aiming at the player before attacking.
#define ENEMY_STATE_AIMING 5

//Enemy has been hit by the player.
#define ENEMY_STATE_HURT 6

//Enemy is sidestepping to avoid player attacks.
#define ENEMY_STATE_EVADING 7

#define ENEMY_FRAME_IDLE 0
#define ENEMY_FRAME_WALK_R1 1
#define ENEMY_FRAME_WALK_R2 2
#define ENEMY_FRAME_WALK_L1 3
#define ENEMY_FRAME_WALK_L2 4
#define ENEMY_FRAME_HURT 5
#define ENEMY_FRAME_AIM 6
#define ENEMY_FRAME_SHOOT 7


#define ENEMY_TYPE_CIV 0
#define ENEMY_TYPE_MER 1
#define ENEMY_TYPE_SGR 2
#define ENEMY_TYPE_HVY 3

#define SPRITE_SLOT_CIV 0
#define SPRITE_SLOT_MER 1
#define SPRITE_SLOT_SGR 2
#define SPRITE_SLOT_HVY 3

#define MAX_ENEMIES 64

typedef struct enemy_t
{
    f16 x, y; //Position X, Y
    u8 targetX, targetY; //If searching, destination cell.
    u8 type; //Enemy type. ENEMY_TYPE_CIV, ENEMY_TYPE_MER etc
    u8 state; //ENEMY_STATE_IDLE, ENEMY_STATE_SEARCHING etc. State of enemy.
    u8 spriteId; //Slot of sprite.
    u8 spriteFrame; //Current frame of sprite to draw.
    u8 health; //Heath of enemy.
    u8 stateCounter; //Decrements per AI tick. Once 0, choose new state and set new stateCounter.
} enemy_t;

extern enemy_t enemyList[MAX_ENEMIES];

u16 getEnemyCell(u16 x, u16 y, s8 cell);
enemy_t* getEnemy(u16 id);
void runAI();
void resetEnemy();


#endif
