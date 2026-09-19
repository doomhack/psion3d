#include <plib.h>	/* p_atos for the counters in the title bars */

#include "menu.h"
#include "ui.h"
#include "mission.h"
#include "game_map.h"
#include "player.h"
#include "automap.h"

/*  Geometry shared by every screen, from the design artboards: a 20 row
    title bar, an 18 row footer under a rule at 142, 13px body text on a
    19px pitch and 16px headings on 22. */
#define TITLE_H 20
#define FOOTER_Y 142
#define ROW_H 19
#define ROW_PITCH 20	/* 19 plus the 1px gap between rows */
#define BODY_H 13
#define HEAD_H 16

/*  Near buffers. menuText holds the copy of whichever level text the open
   screen shows (a briefing, an objective's brief), laid out into lines by
   layoutText. 1280 bytes takes a 200 word briefing; longer ones are cut. */
#define MENU_TEXT_MAX 1280
#define MENU_LINES_MAX 48
#define MENU_STR_MAX 40

static char menuText[MENU_TEXT_MAX];
static u16 lineStart[MENU_LINES_MAX];
static u8 lineLen[MENU_LINES_MAX];
static u8 lineCount = 0;
static char menuStr[MENU_STR_MAX];
static char menuStr2[MENU_STR_MAX];

u8 menuMission = 0;

static u8 screen = MENU_MAIN;
static u8 cursor = 0;	/* the highlighted row of whatever list is open */
static u8 first = 0;	/* the first visible row of a scrolling list */
static u8 mapTop = 0;	/* the first map row the automap shows */
static u8 abortChoice = 1;	/* 0 Abort, 1 Cancel */

#define MAIN_ITEMS 2
#define SELECT_VISIBLE 5
#define BRIEF_LINES 5	/* body lines per page, under the fixed location line */
#define OBJ_BRIEF_LINES 6
/*  The objectives list column. The design has 186 and a rule at 204; the
    device font is a tenth wider than the design's, and 20px more keeps a
    three word title whole rather than cut to an ellipsis. */
#define OBJ_LIST_W 206
#define OBJ_RULE_X 224
#define PAUSE_ITEMS 4
#define MAP_ROWS 26

static const char *mainItems[MAIN_ITEMS] =
{
	"Select Mission", "Exit"
};

static const char *pauseItems[PAUSE_ITEMS] =
{
	"Resume", "Objectives", "Map", "Abort Mission"
};

/* --------------------------------------------------------------- strings */

static u16 slen(const char *s)
{
	u16 n = 0;

	while(s[n])
		n++;

	return n;
}

static void upperTo(char *dst, const char *src, const u16 dstLen)
{
	u16 i;

	for(i = 0; i + 1 < dstLen && src[i]; i++)
		dst[i] = (src[i] >= 'a' && src[i] <= 'z') ? (char)(src[i] - ('a' - 'A')) : src[i];

	dst[i] = 0;
}

static void copyTo(char *dst, const char *src, const u16 dstLen)
{
	u16 i;

	for(i = 0; i + 1 < dstLen && src[i]; i++)
		dst[i] = src[i];

	dst[i] = 0;
}

/*  The text after the last comma of s, minus the spaces after it, or all
    of s when it has no comma. */
static const char *lastField(const char *s)
{
	const char *p = s;
	const char *field = s;

	for(; *p; p++)
		if(*p == ',')
			field = p + 1;

	while(*field == ' ')
		field++;

	return field;
}

/*  "a / b", two digits each, as the mission counter shows it. */
static void counter2(char *dst, const u16 a, const u16 b)
{
	dst[0] = (char)('0' + (a / 10) % 10);
	dst[1] = (char)('0' + a % 10);
	dst[2] = ' ';
	dst[3] = '/';
	dst[4] = ' ';
	dst[5] = (char)('0' + (b / 10) % 10);
	dst[6] = (char)('0' + b % 10);
	dst[7] = 0;
}

/*  The middle dot the design puts between a title and its qualifier. The
    ROM fonts are code page 850, where it is 0xFA; menu_pc.cpp maps that byte
    back to U+00B7. */
#define MIDDOT "\xFA"

/*  "<mission title> · <difficulty>" for the pause screens' title bars. */
static void missionQualifier(char *dst, const u16 dstLen)
{
	const char *d = difficultyName(difficulty);
	u16 n, i;

	mapTextCopy(mapInfo.titleOfs, dst, dstLen);
	n = slen(dst);

	if(n + 4 >= dstLen)
		return;

	dst[n++] = ' ';
	dst[n++] = MIDDOT[0];
	dst[n++] = ' ';

	for(i = 0; n + 1 < dstLen && d[i]; i++)
		dst[n++] = d[i];

	dst[n] = 0;
}

/* ------------------------------------------------------------ primitives */

/*  The y to hand uiText for text centred in a row: its middle. The
    platform centres the capitals on it (see ui.h), so no font height is
    needed here. */
static s16 rowMid(const s16 rowY, const s16 rowH)
{
	return (s16)(rowY + (rowH >> 1));
}

static void text(const s16 x, const s16 y, const s16 font, const s16 inverse, const char *s)
{
	uiText(x, y, font, inverse, s, (short)slen(s));
}

static s16 textWidth(const s16 font, const char *s)
{
	return uiTextWidth(font, s, (short)slen(s));
}

static void textRight(const s16 right, const s16 y, const s16 font, const s16 inverse, const char *s)
{
	text((s16)(right - textWidth(font, s)), y, font, inverse, s);
}

static void textCentre(const s16 cx, const s16 y, const s16 font, const s16 inverse, const char *s)
{
	text((s16)(cx - (textWidth(font, s) >> 1)), y, font, inverse, s);
}

/*  Text cut to maxW with an ellipsis, for list rows that may not fit. */
static void textFit(const s16 x, const s16 y, const s16 font, const s16 inverse, const char *s, const s16 maxW)
{
	s16 n = (s16)slen(s);
	s16 dots;

	if(uiTextWidth(font, s, (short)n) <= maxW)
	{
		uiText(x, y, font, inverse, s, (short)n);
		return;
	}

	dots = uiTextWidth(font, "...", 3);

	while(n > 0 && uiTextWidth(font, s, (short)n) + dots > maxW)
		n--;

	uiText(x, y, font, inverse, s, (short)n);
	uiText((s16)(x + uiTextWidth(font, s, (short)n)), y, font, inverse, "...", 3);
}

/*  The ROM fonts have no arrow glyphs, so the key hints draw their own:
    7px triangles, up and down side by side or left and right. */
static void arrowsUpDown(const s16 x, const s16 y)
{
	s16 k;

	for(k = 0; k < 4; k++)
	{
		uiFillRect((s16)(x + 3 - k), (s16)(y + k), (s16)(1 + k + k), 1);
		uiFillRect((s16)(x + 9 + k), (s16)(y + k), (s16)(7 - k - k), 1);
	}
}

static void arrowsLeftRight(const s16 x, const s16 y)
{
	s16 k;

	for(k = 0; k < 4; k++)
	{
		uiFillRect((s16)(x + k), (s16)(y + 3 - k), 1, (s16)(1 + k + k));
		uiFillRect((s16)(x + 9 + k), (s16)(y + k), 1, (s16)(7 - k - k));
	}
}

#define ARROWS_NONE 0
#define ARROWS_UP_DOWN 1
#define ARROWS_LEFT_RIGHT 2

/*  One "keys action" item of a footer at *x, which advances past it. */
static void footerItem(s16 *x, const s16 y, const u8 arrows, const char *label)
{
	const s16 top = rowMid(y, 18);

	if(arrows != ARROWS_NONE)
	{
		if(arrows == ARROWS_UP_DOWN)
			arrowsUpDown(*x, (s16)(top - 2));
		else
			arrowsLeftRight(*x, (s16)(top - 3));

		*x += 16 + 4;
	}

	text(*x, top, UI_FONT_BODY, FALSE, label);
	*x += textWidth(UI_FONT_BODY, label) + 14;
}

static void titleBar(const char *left, const char *right)
{
	uiFillRect(0, 0, UI_W, TITLE_H);
	text(8, rowMid(0, TITLE_H), UI_FONT_BODY_BOLD, TRUE, left);

	if(right)
		textRight((s16)(UI_W - 8), rowMid(0, TITLE_H), UI_FONT_BODY, TRUE, right);
}

static void footerRule(void)
{
	uiLine(0, FOOTER_Y, UI_W - 1, FOOTER_Y);
}

/*  A 6px wide scrollbar: the box and a thumb sized and placed by what is
    visible of the total. */
static void scrollbar(const s16 x, const s16 y, const s16 h, const u16 total, const u16 firstRow, const u16 visible)
{
	s16 inner = (s16)(h - 2);
	s16 th, ty;

	uiBox(x, y, 6, h);

	if(total == 0)
		return;

	if(total <= visible)
	{
		uiFillRect((s16)(x + 1), (s16)(y + 1), 4, inner);
		return;
	}

	th = (s16)((s32)inner * visible / total);

	if(th < 4)
		th = 4;

	ty = (s16)((s32)(inner - th) * firstRow / (total - visible));
	uiFillRect((s16)(x + 1), (s16)(y + 1 + ty), 4, th);
}

/*  A highlighted list row: the bar and inverse text, or plain text. */
static void listRow(const s16 x, const s16 y, const s16 w, const s16 font, const u16 selected, const char *label)
{
	const s16 top = rowMid(y, (font == UI_FONT_HEAD_BOLD) ? 22 : ROW_H);

	if(selected)
		uiFillRect(x, y, w, (font == UI_FONT_HEAD_BOLD) ? 22 : ROW_H);

	text((s16)(x + ((font == UI_FONT_HEAD_BOLD) ? 8 : 6)), top, font, (s16)selected, label);
}

/*  A dialog with the design's offset shadow, border and title bar. Returns
    nothing; the caller lays out inside (x+1, y+21) .. (x+w-1, y+h-1). */
static void dialog(const s16 x, const s16 y, const s16 w, const s16 h, const char *title)
{
	uiFillRect((s16)(x + 3), (s16)(y + 3), w, h);
	uiClearRect(x, y, w, h);
	uiBox(x, y, w, h);
	uiFillRect((s16)(x + 1), (s16)(y + 1), (s16)(w - 2), TITLE_H);
	text((s16)(x + 9), rowMid((s16)(y + 1), TITLE_H), UI_FONT_BODY_BOLD, TRUE, title);
}

/* ------------------------------------------------------------- word wrap */

/*  Break menuText into lines no wider than width, at spaces, with '\n' a
    forced break. Greedy: a line grows a word at a time and is measured once
    per word, so a briefing costs a few dozen uiTextWidth calls. */
static void layoutText(const s16 font, const s16 width)
{
	u16 pos = 0;
	u16 start, end, wordEnd;

	lineCount = 0;

	while(menuText[pos] && lineCount < MENU_LINES_MAX)
	{
		/* A run of spaces would otherwise read as an empty word for ever. */
		while(menuText[pos] == ' ')
			pos++;

		if(!menuText[pos])
			break;

		start = pos;
		end = pos;

		while(TRUE)
		{
			/* The next word runs to a space, a break or the end. */
			wordEnd = end;

			if(wordEnd > start)
				wordEnd++;	/* over the space before this word */

			while(menuText[wordEnd] && menuText[wordEnd] != ' ' && menuText[wordEnd] != '\n')
				wordEnd++;

			if(uiTextWidth(font, &menuText[start], (short)(wordEnd - start)) > width && end > start)
				break;	/* keep the line as it was */

			end = wordEnd;

			if(menuText[end] != ' ')
				break;	/* '\n' or the end */
		}

		lineStart[lineCount] = start;
		lineLen[lineCount] = (u8)((end - start) > 255 ? 255 : (end - start));
		lineCount++;

		pos = end;

		if(menuText[pos] == ' ' || menuText[pos] == '\n')
			pos++;
	}
}

static void drawLines(const s16 x, const s16 y, const s16 font, const u16 firstLine, const u16 maxLines)
{
	u16 i;

	for(i = 0; i < maxLines && firstLine + i < lineCount; i++)
		uiText(x, (s16)(y + i * ROW_H), font, FALSE, &menuText[lineStart[firstLine + i]], lineLen[firstLine + i]);
}

/* ----------------------------------------------------------- open / keys */

static void loadObjectiveBrief(void)
{
	if(cursor < mapInfo.objectiveCount)
		mapTextCopy(mapInfo.objectiveBriefOfs[cursor], menuText, MENU_TEXT_MAX);
	else
		menuText[0] = 0;

	layoutText(UI_FONT_BODY, (s16)(468 - (OBJ_RULE_X + 12)));
}

void menuOpen(const u8 s)
{
	screen = s;

	switch(screen)
	{
	case MENU_MAIN:
		cursor = 0;
		break;

	case MENU_SELECT:
		if(menuMission >= missionCount)
			menuMission = 0;

		cursor = menuMission;
		first = (cursor >= SELECT_VISIBLE) ? (u8)(cursor - SELECT_VISIBLE + 1) : 0;
		break;

	case MENU_BRIEFING:
		mapTextCopy(mapInfo.briefingOfs, menuText, MENU_TEXT_MAX);
		layoutText(UI_FONT_BODY, 444);
		first = 0;
		break;

	case MENU_OBJECTIVES:
		cursor = 0;
		loadObjectiveBrief();
		break;

	case MENU_PAUSE:
		cursor = 0;
		break;

	case MENU_ABORT:
		abortChoice = 1;
		copyTo(menuText, "All progress in this mission will be lost.", MENU_TEXT_MAX);
		layoutText(UI_FONT_BODY, 214);
		break;

	case MENU_PAUSE_OBJECTIVES:
		break;

	case MENU_MAP:
		/* Open with the player's row in the middle of the window. */
		{
			s16 row = (s16)(fp2int(player.pos.y) - (MAP_ROWS >> 1));

			if(row < 0)
				row = 0;

			if(row > MAP_Y - MAP_ROWS)
				row = MAP_Y - MAP_ROWS;

			mapTop = (u8)row;
		}
		break;
	}
}

static u16 listMove(const u16 key, const u8 count)
{
	if(count == 0)
		return FALSE;

	if(key == UI_KEY_UP && cursor > 0)
	{
		cursor--;
		return TRUE;
	}

	if(key == UI_KEY_DOWN && cursor + 1 < count)
	{
		cursor++;
		return TRUE;
	}

	return FALSE;
}

static u16 keyMain(const u16 key)
{
	if(listMove(key, MAIN_ITEMS))
		return MENU_ACTION_REDRAW;

	if(key == UI_KEY_ESC)
		return MENU_ACTION_QUIT;

	if(key == UI_KEY_ENTER)
	{
		if(cursor == 0)
		{
			menuOpen(MENU_SELECT);
			return MENU_ACTION_REDRAW;
		}

		return MENU_ACTION_QUIT;
	}

	return MENU_ACTION_NONE;
}

static u16 keySelect(const u16 key)
{
	if(listMove(key, missionCount))
	{
		menuMission = cursor;

		if(cursor < first)
			first = cursor;

		if(cursor >= first + SELECT_VISIBLE)
			first = (u8)(cursor - SELECT_VISIBLE + 1);

		return MENU_ACTION_REDRAW;
	}

	if(key == UI_KEY_LEFT && difficulty > 0)
	{
		difficulty--;
		return MENU_ACTION_REDRAW;
	}

	if(key == UI_KEY_RIGHT && difficulty + 1 < DIFFICULTY_COUNT)
	{
		difficulty++;
		return MENU_ACTION_REDRAW;
	}

	if(key == UI_KEY_ESC)
	{
		menuOpen(MENU_MAIN);
		return MENU_ACTION_REDRAW;
	}

	if(key == UI_KEY_ENTER && missionCount > 0)
	{
		/* The briefing reads the level text, so parse the file now. The
		   level's assets are not loaded until the mission starts. */
		if(loadMapFile(missionMapId(menuMission)))
			menuOpen(MENU_BRIEFING);

		return MENU_ACTION_REDRAW;
	}

	return MENU_ACTION_NONE;
}

static u16 keyBriefing(const u16 key)
{
	u8 last = (lineCount > BRIEF_LINES) ? (u8)(lineCount - BRIEF_LINES) : 0;

	if(key == UI_KEY_UP && first > 0)
	{
		first--;
		return MENU_ACTION_REDRAW;
	}

	if(key == UI_KEY_DOWN && first < last)
	{
		first++;
		return MENU_ACTION_REDRAW;
	}

	if(key == UI_KEY_SPACE)
	{
		/* Page down, wrapping to the top from the last page. */
		if(first >= last)
			first = 0;
		else if(first + BRIEF_LINES > last)
			first = last;
		else
			first += BRIEF_LINES;

		return MENU_ACTION_REDRAW;
	}

	if(key == UI_KEY_ESC)
	{
		menuOpen(MENU_SELECT);
		return MENU_ACTION_REDRAW;
	}

	if(key == UI_KEY_ENTER)
	{
		menuOpen(MENU_OBJECTIVES);
		return MENU_ACTION_REDRAW;
	}

	return MENU_ACTION_NONE;
}

static u16 keyObjectives(const u16 key)
{
	if(listMove(key, mapInfo.objectiveCount))
	{
		loadObjectiveBrief();
		return MENU_ACTION_REDRAW;
	}

	if(key == UI_KEY_ESC)
	{
		menuOpen(MENU_BRIEFING);
		return MENU_ACTION_REDRAW;
	}

	if(key == UI_KEY_ENTER)
		return MENU_ACTION_START_MISSION;

	return MENU_ACTION_NONE;
}

static u16 keyPause(const u16 key)
{
	if(listMove(key, PAUSE_ITEMS))
		return MENU_ACTION_REDRAW;

	if(key == UI_KEY_ESC)
		return MENU_ACTION_RESUME;

	if(key == UI_KEY_ENTER)
	{
		switch(cursor)
		{
		case 0:
			return MENU_ACTION_RESUME;
		case 1:
			menuOpen(MENU_PAUSE_OBJECTIVES);
			break;
		case 2:
			menuOpen(MENU_MAP);
			break;
		default:
			menuOpen(MENU_ABORT);
			break;
		}

		return MENU_ACTION_REDRAW;
	}

	return MENU_ACTION_NONE;
}

static u16 keyAbort(const u16 key)
{
	if(key == UI_KEY_LEFT || key == UI_KEY_RIGHT)
	{
		abortChoice = (u8)(1 - abortChoice);
		return MENU_ACTION_REDRAW;
	}

	if(key == UI_KEY_ENTER && abortChoice == 0)
		return MENU_ACTION_ABORT;

	if(key == UI_KEY_ENTER || key == UI_KEY_ESC)
	{
		menuOpen(MENU_PAUSE);
		cursor = PAUSE_ITEMS - 1;
		return MENU_ACTION_REDRAW;
	}

	return MENU_ACTION_NONE;
}

static u16 keyPauseObjectives(const u16 key)
{
	if(key == UI_KEY_ESC)
	{
		menuOpen(MENU_PAUSE);
		cursor = 1;
		return MENU_ACTION_REDRAW;
	}

	return MENU_ACTION_NONE;
}

static u16 keyMap(const u16 key)
{
	if(key == UI_KEY_UP && mapTop > 0)
	{
		mapTop--;
		return MENU_ACTION_REDRAW;
	}

	if(key == UI_KEY_DOWN && mapTop < MAP_Y - MAP_ROWS)
	{
		mapTop++;
		return MENU_ACTION_REDRAW;
	}

	if(key == UI_KEY_ESC)
	{
		menuOpen(MENU_PAUSE);
		cursor = 2;
		return MENU_ACTION_REDRAW;
	}

	return MENU_ACTION_NONE;
}

u16 menuKey(const u16 uiKey)
{
	switch(screen)
	{
	case MENU_MAIN:
		return keyMain(uiKey);
	case MENU_SELECT:
		return keySelect(uiKey);
	case MENU_BRIEFING:
		return keyBriefing(uiKey);
	case MENU_OBJECTIVES:
		return keyObjectives(uiKey);
	case MENU_PAUSE:
		return keyPause(uiKey);
	case MENU_ABORT:
		return keyAbort(uiKey);
	case MENU_PAUSE_OBJECTIVES:
		return keyPauseObjectives(uiKey);
	case MENU_MAP:
		return keyMap(uiKey);
	}

	return MENU_ACTION_NONE;
}

/* ------------------------------------------------------------------ draw */

static void drawMain(void)
{
	s16 x, i;

	titleBar("PSION3D", NULL);

	for(i = 0; i < MAIN_ITEMS; i++)
		listRow(12, (s16)(31 + i * 26), 276, UI_FONT_HEAD_BOLD, (u16)(i == cursor), mainItems[i]);

	/* The title art panel, still a placeholder. */
	uiLine(300, TITLE_H, 300, FOOTER_Y - 1);
	uiPatternRect(312, 31, 156, 100);
	uiBox(312, 31, 156, 100);
	{
		const char *label = "[TITLE ART]";
		s16 w = textWidth(UI_FONT_BODY_BOLD, label);
		s16 lx = (s16)(390 - (w >> 1) - 6);

		uiClearRect(lx, 72, (s16)(w + 12), 18);
		uiBox(lx, 72, (s16)(w + 12), 18);
		text((s16)(lx + 6), rowMid(72, 18), UI_FONT_BODY_BOLD, FALSE, label);
	}

	footerRule();
	x = 12;
	footerItem(&x, FOOTER_Y, ARROWS_UP_DOWN, "Move");
	footerItem(&x, FOOTER_Y, ARROWS_NONE, "Enter Select");
	footerItem(&x, FOOTER_Y, ARROWS_NONE, "Esc Quit");
}

static void drawSelect(void)
{
	s16 x, i, y, locW;
	const char *country;
	u16 mx, my;
	s16 cx, cy;

	counter2(menuStr2, (u16)(missionCount ? cursor + 1 : 0), missionCount);
	titleBar("SELECT MISSION", menuStr2);

	for(i = 0; i < SELECT_VISIBLE && first + i < missionCount; i++)
	{
		const u16 sel = (u16)(first + i == cursor);

		y = (s16)(28 + i * ROW_PITCH);

		if(sel)
			uiFillRect(12, y, 220, ROW_H);

		/* The row shows the country - what follows the last comma of a
		   "City, Country" location, as the design lists them - and the full
		   location goes under the map. It keeps its width at the right; the
		   title gets what is left, with an ellipsis if that is not enough. */
		missionLocation((u8)(first + i), menuStr, MENU_STR_MAX);
		country = lastField(menuStr);
		locW = textWidth(UI_FONT_BODY, country);
		textRight(226, rowMid(y, ROW_H), UI_FONT_BODY, (s16)sel, country);

		missionTitle((u8)(first + i), menuStr, MENU_STR_MAX);
		textFit(18, rowMid(y, ROW_H), UI_FONT_BODY_BOLD, (s16)sel, menuStr, (s16)(226 - locW - 8 - 18));
	}

	scrollbar(238, 28, 106, missionCount, first, SELECT_VISIBLE);
	uiLine(252, TITLE_H, 252, FOOTER_Y - 1);

	/* The world map, a dithered stand-in until the ROM image exists, with
	   the mission's mappos marked on it at the artwork's 360x120 scale. */
	uiPatternRect(264, 28, 204, 62);
	uiBox(264, 28, 204, 62);

	if(missionCount)
	{
		missionMapPos(cursor, &mx, &my);

		if(mx > 359)
			mx = 359;

		if(my > 119)
			my = 119;

		cx = (s16)(265 + (s32)mx * 202 / 360);
		cy = (s16)(29 + (s32)my * 60 / 120);

		uiClearRect((s16)(cx - 11), cy, 22, 1);
		uiClearRect(cx, (s16)(cy - 11), 1, 22);
		uiClearRect((s16)(cx - 4), (s16)(cy - 4), 9, 9);
		uiFillRect((s16)(cx - 2), (s16)(cy - 2), 5, 5);

		missionLocation(cursor, menuStr, MENU_STR_MAX);
		textFit(264, rowMid(96, 14), UI_FONT_BODY_BOLD, FALSE, menuStr, 204);
	}

	for(i = 0; i < DIFFICULTY_COUNT; i++)
	{
		const s16 bx = (s16)(264 + i * 69);
		const u16 sel = (u16)(i == difficulty);

		if(sel)
			uiFillRect(bx, 116, 65, 18);
		else
			uiBox(bx, 116, 65, 18);

		textCentre((s16)(bx + 32), rowMid(116, 18), sel ? UI_FONT_BODY_BOLD : UI_FONT_BODY, (s16)sel, difficultyName((u8)i));
	}

	footerRule();
	x = 12;
	footerItem(&x, FOOTER_Y, ARROWS_UP_DOWN, "Mission");
	footerItem(&x, FOOTER_Y, ARROWS_LEFT_RIGHT, "Difficulty");
	footerItem(&x, FOOTER_Y, ARROWS_NONE, "Enter Brief");
	footerItem(&x, FOOTER_Y, ARROWS_NONE, "Esc Back");
}

static void drawBriefing(void)
{
	s16 x;
	u16 pages = (lineCount > BRIEF_LINES) ? (u16)((lineCount + BRIEF_LINES - 1) / BRIEF_LINES) : 1;
	u16 page = (u16)(first / BRIEF_LINES + 1);

	mapTextCopy(mapInfo.titleOfs, menuStr, MENU_STR_MAX);
	upperTo(menuStr2, menuStr, MENU_STR_MAX);
	p_atos(menuStr, "Page %d / %d", page, pages);
	titleBar(menuStr2, menuStr);

	mapTextCopy(mapInfo.locationOfs, menuStr, MENU_STR_MAX);
	textFit(12, rowMid(24, ROW_H), UI_FONT_BODY_BOLD, FALSE, menuStr, 444);
	drawLines(12, rowMid(43, ROW_H), UI_FONT_BODY, first, BRIEF_LINES);

	scrollbar(462, 24, 114, lineCount, first, BRIEF_LINES);

	footerRule();
	x = 12;
	footerItem(&x, FOOTER_Y, ARROWS_UP_DOWN, "Scroll");
	footerItem(&x, FOOTER_Y, ARROWS_NONE, "Space Page");
	footerItem(&x, FOOTER_Y, ARROWS_NONE, "Enter Objectives");
	footerItem(&x, FOOTER_Y, ARROWS_NONE, "Esc Back");
}

static void drawObjectives(void)
{
	s16 x, i, y, nx;
	char num[2];

	mapTextCopy(mapInfo.titleOfs, menuStr, MENU_STR_MAX);
	upperTo(menuStr2, menuStr, MENU_STR_MAX - 14);
	copyTo(menuStr2 + slen(menuStr2), " " MIDDOT " OBJECTIVES", 14);
	p_atos(menuStr, "%d / %d", (u16)(mapInfo.objectiveCount ? cursor + 1 : 0), (u16)mapInfo.objectiveCount);
	titleBar(menuStr2, menuStr);

	num[1] = 0;

	for(i = 0; i < mapInfo.objectiveCount; i++)
	{
		const u16 sel = (u16)(i == cursor);

		y = (s16)(28 + i * ROW_PITCH);

		if(sel)
			uiFillRect(12, y, OBJ_LIST_W, ROW_H);

		num[0] = (char)('1' + i);
		text(18, rowMid(y, ROW_H), UI_FONT_BODY, (s16)sel, num);
		nx = (s16)(18 + textWidth(UI_FONT_BODY, num) + 6);

		mapTextCopy(mapInfo.objectiveOfs[i], menuStr, MENU_STR_MAX);
		textFit(nx, rowMid(y, ROW_H), UI_FONT_BODY_BOLD, (s16)sel, menuStr, (s16)(12 + OBJ_LIST_W - 6 - nx));
	}

	uiLine(OBJ_RULE_X, TITLE_H, OBJ_RULE_X, FOOTER_Y - 1);
	drawLines(OBJ_RULE_X + 12, rowMid(28, ROW_H), UI_FONT_BODY, 0, OBJ_BRIEF_LINES);

	footerRule();
	x = 12;
	footerItem(&x, FOOTER_Y, ARROWS_UP_DOWN, "Objective");
	footerItem(&x, FOOTER_Y, ARROWS_NONE, "Enter Begin Mission");
	footerItem(&x, FOOTER_Y, ARROWS_NONE, "Esc Back");
}

/*  The pause dialog. withCursor is off when the abort dialog sits on top. */
static void drawPauseDialog(const u16 withCursor)
{
	s16 x, i;

	uiPatternRect(0, 0, UI_W, UI_H);
	dialog(94, 12, 288, 132, "PAUSED");

	for(i = 0; i < PAUSE_ITEMS; i++)
		listRow(107, (s16)(40 + i * ROW_PITCH), 262, UI_FONT_BODY_BOLD, (u16)(withCursor && i == cursor), pauseItems[i]);

	uiLine(95, 125, 380, 125);
	x = 107;
	footerItem(&x, 125, ARROWS_UP_DOWN, "Move");
	footerItem(&x, 125, ARROWS_NONE, "Enter Select");
	footerItem(&x, 125, ARROWS_NONE, "Esc Resume");
}

static void drawPause(void)
{
	drawPauseDialog(TRUE);
}

static void drawAbort(void)
{
	s16 x;

	/* The pause menu behind, on Abort Mission, which is how it got here. */
	cursor = PAUSE_ITEMS - 1;
	drawPauseDialog(TRUE);

	dialog(120, 24, 240, 112, "ABORT MISSION");
	drawLines(133, rowMid(53, ROW_H), UI_FONT_BODY, 0, 2);

	/* Abort, then Cancel, right aligned; the chosen one is filled. */
	if(abortChoice == 0)
		uiFillRect(227, 95, 56, 18);
	else
		uiBox(227, 95, 56, 18);

	textCentre(255, rowMid(95, 18), (abortChoice == 0) ? UI_FONT_BODY_BOLD : UI_FONT_BODY, (s16)(abortChoice == 0), "Abort");

	if(abortChoice == 1)
		uiFillRect(291, 95, 56, 18);
	else
		uiBox(291, 95, 56, 18);

	textCentre(319, rowMid(95, 18), (abortChoice == 1) ? UI_FONT_BODY_BOLD : UI_FONT_BODY, (s16)(abortChoice == 1), "Cancel");

	uiLine(121, 119, 358, 119);
	x = 131;
	footerItem(&x, 119, ARROWS_NONE, "Enter Confirm");
	footerItem(&x, 119, ARROWS_NONE, "Esc Back");
}

/*  The 9x9 objective mark: a box, filled when complete, crossed when failed. */
static void objectiveMark(const s16 x, const s16 y, const u8 state)
{
	if(state == OBJECTIVE_FAILED)
	{
		uiLine(x, y, (s16)(x + 8), (s16)(y + 8));
		uiLine((s16)(x + 8), y, x, (s16)(y + 8));
		return;
	}

	uiBox(x, y, 9, 9);

	if(state == OBJECTIVE_COMPLETE)
		uiFillRect((s16)(x + 2), (s16)(y + 2), 5, 5);
}

static void drawPauseObjectives(void)
{
	s16 x, i, y;

	missionQualifier(menuStr2, MENU_STR_MAX);
	titleBar("OBJECTIVES", menuStr2);

	for(i = 0; i < mapInfo.objectiveCount; i++)
	{
		const u8 state = objectiveState[i];
		const char *label = (state == OBJECTIVE_COMPLETE) ? "Complete" :
			(state == OBJECTIVE_FAILED) ? "Failed" : "Incomplete";

		y = (s16)(28 + i * ROW_PITCH);

		objectiveMark(18, (s16)(y + 5), state);

		mapTextCopy(mapInfo.objectiveOfs[i], menuStr, MENU_STR_MAX);
		textFit(35, rowMid(y, ROW_H), UI_FONT_BODY, FALSE, menuStr, 340);

		textRight(462, rowMid(y, ROW_H),
			(state == OBJECTIVE_INCOMPLETE) ? UI_FONT_BODY : UI_FONT_BODY_BOLD, FALSE, label);
	}

	footerRule();
	x = 12;
	footerItem(&x, FOOTER_Y, ARROWS_NONE, "Esc Back");
}

/*  One line of the map key: a 9x9 mark and its label. */
static void keyRow(const s16 y, const char *label)
{
	text(313, rowMid(y, 15), UI_FONT_BODY, FALSE, label);
}

static void drawMap(void)
{
	s16 x, k;

	missionQualifier(menuStr2, MENU_STR_MAX);
	titleBar("MAP", menuStr2);

	automapDraw(mapTop);
	uiBox(12, 28, 258, 106);
	uiBlitMap(13, 29, 0, 256, 104);

	scrollbar(280, 28, 106, MAP_Y, mapTop, MAP_ROWS);

	/* Solid */
	uiFillRect(296, 31, 9, 9);
	keyRow(28, "Solid");

	/* See through */
	uiGreyRect(296, 48, 9, 9);
	keyRow(45, "See through");

	/* Way through: a grey ring */
	uiGreyRect(297, 66, 7, 2);
	uiGreyRect(297, 71, 7, 2);
	uiGreyRect(297, 66, 2, 7);
	uiGreyRect(302, 66, 2, 7);
	keyRow(62, "Way through");

	/* Locked */
	uiGreyRect(296, 82, 9, 9);
	uiFillRect(299, 85, 3, 3);
	keyRow(79, "Locked");

	/* You: a triangle pointing right */
	for(k = 0; k < 4; k++)
		uiFillRect((s16)(298 + k), (s16)(100 + k), 1, (s16)(7 - k - k));

	keyRow(96, "You");

	p_atos(menuStr, "Rows %d-%d of %d", (u16)(mapTop + 1), (u16)(mapTop + MAP_ROWS), (u16)MAP_Y);
	text(296, rowMid(121, BODY_H), UI_FONT_BODY, FALSE, menuStr);

	footerRule();
	x = 12;
	footerItem(&x, FOOTER_Y, ARROWS_UP_DOWN, "Pan");
	footerItem(&x, FOOTER_Y, ARROWS_NONE, "Esc Back");
}

void menuDraw(void)
{
	uiClear();

	switch(screen)
	{
	case MENU_MAIN:
		drawMain();
		break;
	case MENU_SELECT:
		drawSelect();
		break;
	case MENU_BRIEFING:
		drawBriefing();
		break;
	case MENU_OBJECTIVES:
		drawObjectives();
		break;
	case MENU_PAUSE:
		drawPause();
		break;
	case MENU_ABORT:
		drawAbort();
		break;
	case MENU_PAUSE_OBJECTIVES:
		drawPauseObjectives();
		break;
	case MENU_MAP:
		drawMap();
		break;
	}
}
