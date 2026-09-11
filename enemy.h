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

//Enemy is falling to the ground.
#define ENEMY_STATE_DYING 8

//Enemy is dead and cannot leave this state.
#define ENEMY_STATE_DEAD 9

//Civilian is wandering the map, avoiding the player.
#define ENEMY_STATE_WANDER 10

//Enemy is running away from the player.
#define ENEMY_STATE_FLEEING 11

#define ENEMY_FRAME_IDLE 0
#define ENEMY_FRAME_WALK_R1 1
#define ENEMY_FRAME_WALK_R2 2
#define ENEMY_FRAME_AIM 3
#define ENEMY_FRAME_SHOOT 4
#define ENEMY_FRAME_HURT 5
#define ENEMY_FRAME_DYING 6
#define ENEMY_FRAME_DEATH 7


#define ENEMY_TYPE_CIV 0
#define ENEMY_TYPE_MER 1
#define ENEMY_TYPE_SGR 2
#define ENEMY_TYPE_HVY 3

#define MAX_ENEMIES 64

typedef struct enemystats_t
{
    f16 moveSpeed; //Move and sidestep speed meters per second
    u8 evadeChance; //0..255 chance of going into evade state.
    u8 fleeChance; //0..255 chance of going into flee state after a pain state.
    u8 health; //Starting health.
    u8 staggerDamage; //Hits below this land but do not interrupt. 0 flinches at everything.
    u8 damage; //How damage a shot does to the player.
    u8 accuracy; //..255 chance of enemy hitting the player.
    u8 aimTicks; //Wind-up before a burst, and after every reposition. 32 ticks per second.
    u8 attackTicks; //Interval between rounds within a burst.
    u8 repositionChance; //0..255 chance each round ends the burst with a sidestep. Sets burst length and movement together.
    u8 spriteId; //Sprite Slot.
} enemystats_t;

typedef struct enemy_t
{
    f16 x, y; //Position X, Y
    f16 moveTargetX, moveTargetY; //Visual movement target.
    u8 cellX, cellY; //Current map cell containing the visual position.
    u8 targetX, targetY; //If searching, destination cell.
    u8 type; //Enemy type. ENEMY_TYPE_CIV, ENEMY_TYPE_MER etc
    u8 state; //ENEMY_STATE_IDLE, ENEMY_STATE_SEARCHING etc. State of enemy.
    u8 spriteFrame; //Current frame of sprite to draw.
    u8 spriteMirrored; //Draw the current frame horizontally mirrored.
    u8 health; //Heath of enemy.
    u8 stateCounter; //Decrements per AI tick. Once 0, choose new state and set new stateCounter.
    u8 wanderDir; //Current wander heading, 0..3, indexes wanderStepX/wanderStepY.
    u8 stateCells; //Move periods left in the current activity. FLEEING counts flight, SEARCHING counts patience. Set on entry to either.
    u8 hurtResumeState; //State interrupted by HURT, restored on exit if it was an aim or attack.
    u8 hurtResumeCounter; //stateCounter to restore alongside hurtResumeState.
    u8 aimMemory; //Aim ticks still owed when the player last ducked out of sight.
    u8 aimMemoryAge; //Ticks since then, saturating at ENEMY_AIM_MEMORY_TICKS (= expired).
    u16 underCell; //The map cell this enemy is standing on, restored exactly when it leaves.
    const enemystats_t* enemyStats; //Enemy stats.
} enemy_t;

extern enemy_t enemyList[MAX_ENEMIES];

u16 getEnemyCell(u16 x, u16 y, s8 cell);
enemy_t* getEnemy(u16 id);
u16 enemyBlocksPosition(f16 x, f16 y);
void damageEnemy(u16 id, u8 damage);
void alertEnemies(const u8 x, const u8 y);
void runAI(void);
void resetEnemy(void);


#endif
