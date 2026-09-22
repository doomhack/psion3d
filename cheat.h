#ifndef CHEAT_H
#define CHEAT_H

#include "fp_types.h"

/*  Cheats (TASKS.md task 24): one bit each in a u16, toggled on the Cheats
    screen of the main menu.

    Every cheat is a value change at spawn time or a branch in per-tick code;
    none reaches the per-ray or per-column paths. Mirror mode and tiny
    enemies touch the renderer, but per sprite and once a mission, not per
    pixel. The bit number is also the cheat's row on the Cheats screen. */

#define CHEAT_INVINCIBLE 0x0001	/* hurtPlayer takes no health */
#define CHEAT_ALL_WEAPONS 0x0002	/* initPlayer gives every weapon and a pickup of each ammo */
#define CHEAT_INFINITE_AMMO 0x0004	/* a shot takes no round from the pool */
#define CHEAT_TURBO 0x0008	/* the player moves and turns x1.5 */
#define CHEAT_ONE_SHOT 0x0010	/* enemies spawn with 1 health */
#define CHEAT_PERFECT_AIM 0x0020	/* no spread on the player's shots */
#define CHEAT_RAPID_FIRE 0x0040	/* a cooldown of 1 tick after every shot */
#define CHEAT_INVISIBLE 0x0080	/* enemies never see the player; they still hear */
#define CHEAT_PACIFIST 0x0100	/* every enemy's AI decides as a civilian's does */
#define CHEAT_SLOW_ENEMIES 0x0200	/* runAI on even ticks only */
#define CHEAT_FAST_ENEMIES 0x0400	/* runAI twice a tick */
#define CHEAT_MIRROR 0x0800	/* the view, the sprites, the controls and the automap flipped */
#define CHEAT_TINY_ENEMIES 0x1000	/* enemy sprites projected at half size */
#define CHEAT_ALL_MERCS 0x2000	/* E, F and G all spawn as mercenaries */
#define CHEAT_ALL_SOLDIERS 0x4000	/* ... as soldiers */
#define CHEAT_ALL_HEAVIES 0x8000	/* ... as heavies */

#define CHEAT_COUNT 16
#define CHEAT_ALL 0xffff

/*  The radio groups: turning one on turns the others in its group off. */
#define CHEAT_GROUP_SPEED (CHEAT_SLOW_ENEMIES | CHEAT_FAST_ENEMIES)
#define CHEAT_GROUP_TYPE (CHEAT_ALL_MERCS | CHEAT_ALL_SOLDIERS | CHEAT_ALL_HEAVIES)

/*  What the Cheats screen has on. Process lifetime, like the Options. */
extern u16 cheatFlags;

/*  What the mission in play runs with: cheatFlags as they were when it
    started, or none for the benchmark map. Every hook tests this rather
    than cheatFlags, so nothing changes under a mission in progress. */
extern u16 cheatActive;

/*  Which cheats may be toggled. All of them for now; the unlock criteria
    (a mission, a difficulty and a time) come with the save file. */
extern u16 cheatUnlocked;

/*  Take cheatFlags into play for a mission on mapId. Called before the level
    loads, since the spawn-time cheats act as enemies are placed. */
void cheatBegin(const u8 mapId);

/*  Turn cheat i (0 .. CHEAT_COUNT-1) on or off, keeping the radio groups.
    Returns FALSE, changing nothing, for a locked cheat. */
u16 cheatToggle(const u8 i);

const char *cheatName(const u8 i);
const char *cheatInfo(const u8 i);

/*  The play key mask as the player means it: under the mirror, left and
    right swap, turning and strafing both. */
u16 cheatKeys(const u16 keys);

/*  The enemy type a map character spawns as, under the type cheats. */
u8 cheatEnemyType(const u8 type);

#endif
