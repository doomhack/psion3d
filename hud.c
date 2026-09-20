#include <plib.h>	/* p_atos for the numbers */

#include "hud.h"
#include "ui.h"
#include "player.h"
#include "mission.h"
#include "settings.h"
#include "game_map.h"

/*  Geometry from the HUD artboard. Each panel is 120 wide with a 1px rule
    on its inner edge; strips are 16 rows of inverse bold capitals.

    Everything that can change while playing is a cell: a box that one
    uiTextBox call replaces. hudUpdate compares each value with the one on
    screen and redraws only the cells that moved, straight into the window
    - a shot costs one message for its ammo cell, a hit one for the number
    and one or two for the bar segments. The strips, rules, key boxes and
    weapon names are static and only drawn by hudDraw, the full repaint the
    platform runs when the window is uncovered. */
#define HUD_PANEL_W 120
#define HUD_RIGHT_X 360
#define HUD_STRIP_H 16
#define HUD_PAD 6
#define HUD_CELL_X HUD_PAD
#define HUD_CELL_W (HUD_PANEL_W - 1 - HUD_PAD - HUD_PAD)

#define HUD_HEALTH_Y 20	/* the big number, 32 rows */
#define HUD_BIG_H 32
#define HUD_HEALTH_BAR_Y 55	/* ten 9x10 segments, 1px apart */
#define HUD_HEALTH_SEGMENTS 10
#define HUD_OBJ_STRIP_Y 89	/* strip, 4, big number, 4, then the fps row */
#define HUD_OBJ_Y 109
#define HUD_FPS_Y 145	/* 15 rows under a rule */
#define HUD_FPS_CELL_X 40	/* right of the "FPS" label */

#define HUD_WEAPON_Y 20	/* first row; 26 tall on a 28 pitch */
#define HUD_WEAPON_ROW_H 26
#define HUD_WEAPON_PITCH 28
#define HUD_KEY_BOX 14
#define HUD_NAME_X (HUD_RIGHT_X + 1 + HUD_PAD + HUD_KEY_BOX + 4)
#define HUD_AMMO_RIGHT (UI_W - HUD_PAD)
#define HUD_AMMO_GAP 4

#define WEAPON_COUNT 4

static const char *weaponNames[WEAPON_COUNT] =
{
	"Pistol", "SMG", "AK-47", "LMG"
};

/*  What is on screen, so a frame that changes nothing draws nothing. */
typedef struct hudshown_t
{
	u8 health;
	u8 objectivesDone;
	u8 objectiveCount;
	u8 weapon;
	u8 weaponsOwned;
	u8 ammo[AMMO_TYPE_COUNT];
	u8 fps;
	u8 showFps;
	u8 valid;
} hudshown_t;

static hudshown_t shown;
static u8 fpsNow = 0;

static u16 slen(const char *s)
{
	u16 n = 0;

	while(s[n])
		n++;

	return n;
}

static void text(const s16 x, const s16 y, const s16 font, const s16 inverse, const char *s)
{
	uiText(x, y, font, inverse, s, (short)slen(s));
}

static void cell(const s16 x, const s16 y, const s16 w, const s16 h, const s16 font, const s16 inverse, const s16 align, const char *s)
{
	uiTextBox(x, y, w, h, font, inverse, align, s, (short)slen(s));
}

static u8 currentWeapon(void)
{
	return (u8)(player.currentWeapon - &weapons[0]);
}

static u8 objectivesDone(void)
{
	u8 i, n = 0;

	for(i = 0; i < mapInfo.objectiveCount; i++)
		if(objectiveState[i] == OBJECTIVE_COMPLETE)
			n++;

	return n;
}

static void snapshot(hudshown_t *s)
{
	u8 i;

	s->health = player.health;
	s->objectivesDone = objectivesDone();
	s->objectiveCount = mapInfo.objectiveCount;
	s->weapon = currentWeapon();
	s->weaponsOwned = player.weaponsOwned;

	for(i = 0; i < AMMO_TYPE_COUNT; i++)
		s->ammo[i] = player.ammo[i];

	s->fps = fpsNow;
	s->showFps = showFps;
	s->valid = TRUE;
}

void hudInvalidate(void)
{
	shown.valid = FALSE;
}

void hudSetFps(const u8 fps)
{
	fpsNow = fps;
}

/* ----------------------------------------------------------------- cells */

static u8 healthSegments(const u8 health)
{
	u8 filled = (u8)(health / 10);

	/* A segment per ten points, and at least one while alive. */
	if(filled == 0 && health > 0)
		filled = 1;

	return filled;
}

static void healthNumber(const hudshown_t *s)
{
	char buf[8];

	p_atos(buf, "%d", (u16)s->health);
	cell(HUD_CELL_X, HUD_HEALTH_Y, HUD_CELL_W, HUD_BIG_H, UI_FONT_BIG, FALSE, UI_ALIGN_LEFT, buf);
}

/*  Segment interiors from (and including) segment a up to b: filled when
    they should be, cleared otherwise. The outlines are static. */
static void healthSegmentsBetween(u8 a, u8 b, const u8 filled)
{
	u8 k;

	if(a > b)
	{
		k = a;
		a = b;
		b = k;
	}

	for(k = a; k < b && k < HUD_HEALTH_SEGMENTS; k++)
	{
		const s16 x = (s16)(HUD_PAD + 1 + k * 10);

		if(k < filled)
			uiFillRect(x, HUD_HEALTH_BAR_Y + 1, 7, 8);
		else
			uiClearRect(x, HUD_HEALTH_BAR_Y + 1, 7, 8);
	}
}

static void objectivesNumber(const hudshown_t *s)
{
	char buf[8];

	p_atos(buf, "%d / %d", (u16)s->objectivesDone, (u16)s->objectiveCount);
	cell(HUD_CELL_X, HUD_OBJ_Y, HUD_CELL_W, HUD_BIG_H, UI_FONT_BIG, FALSE, UI_ALIGN_LEFT, buf);
}

static void fpsNumber(const hudshown_t *s)
{
	char buf[8];

	p_atos(buf, "%d", (u16)s->fps);
	cell(HUD_FPS_CELL_X, HUD_FPS_Y + 1, HUD_PANEL_W - 1 - HUD_PAD - HUD_FPS_CELL_X, 14, UI_FONT_BODY_BOLD, FALSE, UI_ALIGN_RIGHT, buf);
}

/*  Where a row's ammo cell starts: just past its name, measured in the bold
    face the selected row uses, since the cell replaces everything in its box
    and the ROM Swiss 13 is wider than the design's Helvetica. gTextWidth is
    client-side, so this costs no round trip. */
static s16 ammoCellX(const s16 i)
{
	return (s16)(HUD_NAME_X + uiTextWidth(UI_FONT_BODY_BOLD, weaponNames[i], (short)slen(weaponNames[i])) + HUD_AMMO_GAP);
}

static void ammoCell(const hudshown_t *s, const s16 i)
{
	const s16 y = (s16)(HUD_WEAPON_Y + i * HUD_WEAPON_PITCH);
	const s16 x = ammoCellX(i);
	const u16 sel = (u16)(i == s->weapon);
	const u16 owned = (u16)(i == WEAPON_PISTOL || (s->weaponsOwned & (1 << i)));
	const u8 ammoType = weapons[i].ammoType;
	char buf[12];

	/* The pistol never runs dry; a weapon not yet picked up shows no count
	   at all - but the cell is still replaced, to carry the row's colour. */
	if(!owned)
		buf[0] = 0;
	else if(ammoType == AMMO_TYPE_NONE)
		p_atos(buf, "-");
	else
		p_atos(buf, "%d/%d", (u16)s->ammo[ammoType], (u16)ammoTypes[ammoType].cap);

	cell(x, y, (s16)(HUD_AMMO_RIGHT - x), HUD_WEAPON_ROW_H, UI_FONT_BODY_BOLD, (s16)sel, UI_ALIGN_RIGHT, buf);
}

/*  The 14x14 key box with its digit, white on a highlighted row. */
static void keyBox(const s16 x, const s16 y, const char digit, const u16 inverse)
{
	char buf[2];

	buf[0] = digit;
	buf[1] = 0;

	if(inverse)
	{
		uiClearRect(x, y, HUD_KEY_BOX, 1);
		uiClearRect(x, (s16)(y + HUD_KEY_BOX - 1), HUD_KEY_BOX, 1);
		uiClearRect(x, y, 1, HUD_KEY_BOX);
		uiClearRect((s16)(x + HUD_KEY_BOX - 1), y, 1, HUD_KEY_BOX);
	}
	else
	{
		uiBox(x, y, HUD_KEY_BOX, HUD_KEY_BOX);
	}

	text((s16)(x + ((HUD_KEY_BOX - uiTextWidth(UI_FONT_BODY, buf, 1)) >> 1)),
		(s16)(y + (HUD_KEY_BOX >> 1)), UI_FONT_BODY, (s16)inverse, buf);
}

/*  A whole weapon row: its bar, key box, name and ammo. Drawn in full when
    the selection moves onto or off it. */
static void weaponRow(const hudshown_t *s, const s16 i)
{
	const s16 y = (s16)(HUD_WEAPON_Y + i * HUD_WEAPON_PITCH);
	const s16 mid = (s16)(y + (HUD_WEAPON_ROW_H >> 1));
	const u16 sel = (u16)(i == s->weapon);

	if(sel)
		uiFillRect(HUD_RIGHT_X + 1, y, HUD_PANEL_W - 1, HUD_WEAPON_ROW_H);
	else
		uiClearRect(HUD_RIGHT_X + 1, y, HUD_PANEL_W - 1, HUD_WEAPON_ROW_H);

	keyBox(HUD_RIGHT_X + 1 + HUD_PAD, (s16)(y + ((HUD_WEAPON_ROW_H - HUD_KEY_BOX) >> 1)), (char)('1' + i), sel);
	text(HUD_NAME_X, mid, sel ? UI_FONT_BODY_BOLD : UI_FONT_BODY, (s16)sel, weaponNames[i]);
	ammoCell(s, i);
}

/* ------------------------------------------------------------------ draw */

static void strip(const s16 x, const s16 y, const char *label)
{
	uiFillRect(x, y, HUD_PANEL_W - 1, HUD_STRIP_H);
	text((s16)(x + HUD_PAD), (s16)(y + (HUD_STRIP_H >> 1)), UI_FONT_BODY_BOLD, TRUE, label);
}

void hudDraw(void)
{
	s16 k;

	snapshot(&shown);

	/* Left: strips, the bar's outlines and the fps rule are static. */
	uiClearRect(0, 0, HUD_PANEL_W, UI_H);
	uiLine(HUD_PANEL_W - 1, 0, HUD_PANEL_W - 1, UI_H - 1);

	strip(0, 0, "HEALTH");
	healthNumber(&shown);

	for(k = 0; k < HUD_HEALTH_SEGMENTS; k++)
		uiBox((s16)(HUD_PAD + k * 10), HUD_HEALTH_BAR_Y, 9, 10);

	healthSegmentsBetween(0, HUD_HEALTH_SEGMENTS, healthSegments(shown.health));

	strip(0, HUD_OBJ_STRIP_Y, "OBJECTIVES");
	objectivesNumber(&shown);

	if(shown.showFps)
	{
		uiLine(0, HUD_FPS_Y, HUD_PANEL_W - 2, HUD_FPS_Y);
		text(HUD_PAD, HUD_FPS_Y + 8, UI_FONT_BODY, FALSE, "FPS");
		fpsNumber(&shown);
	}

	/* Right: the strip, then every row. */
	uiClearRect(HUD_RIGHT_X, 0, HUD_PANEL_W, UI_H);
	uiLine(HUD_RIGHT_X, 0, HUD_RIGHT_X, UI_H - 1);

	strip(HUD_RIGHT_X + 1, 0, "WEAPONS");

	for(k = 0; k < WEAPON_COUNT; k++)
		weaponRow(&shown, k);
}

void hudUpdate(void)
{
	hudshown_t now;
	u8 i;

	if(!shown.valid)
	{
		hudDraw();
		return;
	}

	snapshot(&now);

	/* The fps row appears and disappears with the option: a full repaint. */
	if(now.showFps != shown.showFps)
	{
		hudDraw();
		return;
	}

	if(now.health != shown.health)
	{
		healthNumber(&now);
		healthSegmentsBetween(healthSegments(shown.health), healthSegments(now.health), healthSegments(now.health));
	}

	if(now.objectivesDone != shown.objectivesDone || now.objectiveCount != shown.objectiveCount)
		objectivesNumber(&now);

	if(now.showFps && now.fps != shown.fps)
		fpsNumber(&now);

	if(now.weapon != shown.weapon)
	{
		/* Two rows change colour; their ammo cells go with them. */
		weaponRow(&now, shown.weapon);
		weaponRow(&now, now.weapon);
	}

	for(i = 0; i < WEAPON_COUNT; i++)
	{
		const u8 ammoType = weapons[i].ammoType;
		const u16 ownedNow = (u16)((now.weaponsOwned >> i) & 1);
		const u16 ownedThen = (u16)((shown.weaponsOwned >> i) & 1);

		if(i == now.weapon || i == shown.weapon)
			continue;	/* redrawn whole above, if the selection moved */

		if(ownedNow != ownedThen || (ammoType != AMMO_TYPE_NONE && now.ammo[ammoType] != shown.ammo[ammoType]))
			ammoCell(&now, i);
	}

	/* The selected weapon's own ammo, the per-shot case: one cell. */
	if(now.weapon == shown.weapon)
	{
		const u8 ammoType = weapons[now.weapon].ammoType;

		if(ammoType != AMMO_TYPE_NONE && now.ammo[ammoType] != shown.ammo[ammoType])
			ammoCell(&now, now.weapon);
		else if(((now.weaponsOwned ^ shown.weaponsOwned) >> now.weapon) & 1)
			ammoCell(&now, now.weapon);
	}

	shown = now;
}
