#include "cheat.h"
#include "psion3d.h"	/* KEY_* */
#include "enemy.h"
#include "draw.h"
#include "bench.h"

u16 cheatFlags = 0;
u16 cheatActive = 0;
u16 cheatUnlocked = CHEAT_ALL;

/*  In bit order. The names are the Cheats screen's rows and the text is what
    it shows beside the highlighted one; both are near data, so keep them
    short. */
static const char *cheatNames[CHEAT_COUNT] =
{
	"Invincibility", "All Weapons", "Infinite Ammo", "Turbo Mode",
	"One-Shot Kills", "Perfect Aim", "Rapid Fire", "Invisibility",
	"Pacifist", "Slow Enemies", "Fast Enemies", "Mirror Mode",
	"Tiny Enemies", "All Mercs", "All Soldiers", "All Heavies"
};

static const char *cheatText[CHEAT_COUNT] =
{
	"Hits knock you about but do no harm.",
	"Start with all four guns, and ammo.",
	"Firing never uses a round.",
	"Move and turn 50% faster.",
	"Every enemy dies to one round.",
	"No spread: rounds go where you aim.",
	"Sixteen rounds a second, any gun.",
	"Enemies can't see you, but they hear gunfire.",
	"Enemies never attack. They act like civilians.",
	"Enemies act at half speed.",
	"Enemies act at double speed.",
	"The world is flipped left to right.",
	"Enemies are half size.",
	"Soldiers and heavies become mercs.",
	"Mercs and heavies become soldiers.",
	"Mercs and soldiers become heavies."
};

void cheatBegin(const u8 mapId)
{
	/* The benchmark measures the renderer as it ships, whatever is on. */
	cheatActive = (mapId == BENCH_MAP_ID) ? 0 : cheatFlags;

	drawSetMirror((u16)(cheatActive & CHEAT_MIRROR));
}

u16 cheatToggle(const u8 i)
{
	const u16 bit = (u16)(1u << i);

	if(i >= CHEAT_COUNT || !(cheatUnlocked & bit))
		return FALSE;

	if(cheatFlags & bit)
	{
		cheatFlags &= (u16)~bit;
		return TRUE;
	}

	/* The last one toggled in a group wins. */
	if(bit & CHEAT_GROUP_SPEED)
		cheatFlags &= (u16)(CHEAT_ALL ^ CHEAT_GROUP_SPEED);
	else if(bit & CHEAT_GROUP_TYPE)
		cheatFlags &= (u16)(CHEAT_ALL ^ CHEAT_GROUP_TYPE);

	cheatFlags |= bit;

	return TRUE;
}

const char *cheatName(const u8 i)
{
	return (i < CHEAT_COUNT) ? cheatNames[i] : "";
}

const char *cheatInfo(const u8 i)
{
	return (i < CHEAT_COUNT) ? cheatText[i] : "";
}

u16 cheatKeys(const u16 keys)
{
	u16 k;

	if(!(cheatActive & CHEAT_MIRROR))
		return keys;

	k = (u16)(keys & ~(KEY_LEFT | KEY_RIGHT | KEY_STRAFE_LEFT | KEY_STRAFE_RIGHT));

	if(keys & KEY_LEFT)
		k |= KEY_RIGHT;

	if(keys & KEY_RIGHT)
		k |= KEY_LEFT;

	if(keys & KEY_STRAFE_LEFT)
		k |= KEY_STRAFE_RIGHT;

	if(keys & KEY_STRAFE_RIGHT)
		k |= KEY_STRAFE_LEFT;

	return k;
}

u8 cheatEnemyType(const u8 type)
{
	/* Civilians stay civilians, so Pacifist and the objectives about them
	   keep their meaning. */
	if(type == ENEMY_TYPE_CIV)
		return type;

	if(cheatActive & CHEAT_ALL_MERCS)
		return ENEMY_TYPE_MER;

	if(cheatActive & CHEAT_ALL_SOLDIERS)
		return ENEMY_TYPE_SGR;

	if(cheatActive & CHEAT_ALL_HEAVIES)
		return ENEMY_TYPE_HVY;

	return type;
}
