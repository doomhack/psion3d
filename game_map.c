#include <plib.h>	/* loadMap calls p_open / p_read / p_close directly. */

#include "fp_types.h"
#include "game_map.h"
#include "sprite.h"
#include "sprslot.h"
#include "debug.h"
#include "enemy.h"
#include "pickup.h"
#include "decor.h"
#include "walls.h"
#include "level.h"

#define MAP_FILE_NAME_LEN 64

u16 map[MAP_Y][MAP_X];
mapinfo_t mapInfo;

void loadMapData(const u8 mapId)
{
	/* Select the complete wall style for this level here. */
	switch (mapId)
	{
	case 1:
	case 97: /* the benchmark stations (bench.c), measured in the shipped style */
	case 98: /* the showcase corridor in this style, see golden/views.txt */
		drawWall = drawWallLab;
		break;
	default:
		drawWall = drawWallDefault;
		break;
	}

	/* And its script. Separate from the style switch because the showcase
	   corridor shares map 1's look but not its objectives. */
	switch (mapId)
	{
	case 1:
		levelEvent = levelEventMap1;
		break;
	default:
		levelEvent = levelEventNone;
		break;
	}

	loadSprite("sci", SPRITE_SLOT_CIV);
	loadSprite("mer", SPRITE_SLOT_MER);
	loadSprite("sgr", SPRITE_SLOT_SGR);
	loadSprite("hvy", SPRITE_SLOT_HVY);
	loadSprite("hit", SPRITE_SLOT_PARTICLES);

	loadSprite("ppk2", SPRITE_SLOT_PISTOL);
	loadSprite("mp5", SPRITE_SLOT_SMG);
	loadSprite("ak", SPRITE_SLOT_AR);
	loadSprite("m249", SPRITE_SLOT_LMG);

	loadSprite("pup", SPRITE_SLOT_PICKUPS);
	loadSprite("dec", SPRITE_SLOT_DECORATIONS);
}

u16 getCellEncoding(u16 x, u16 y, s8 cell)
{
	switch (cell)
	{
	case '0': // Open space
		return (MAP_MASK_WALK);

	// Walls
	case 'X': // Brick wall
		return (MAP_MASK_WALL | MAP_MASK_SOLID | SET_CELL_TYPE_ID(WALL_TYPE_SOLID));

	case 'P': // Dark brick wall
		return (MAP_MASK_WALL | MAP_MASK_SOLID | SET_CELL_TYPE_ID(WALL_TYPE_DARK));

	case 'W': // Window
		return (MAP_MASK_WALL | SET_CELL_TYPE_ID(WALL_TYPE_WINDOW));

	case 'A': // Archway
		return (MAP_MASK_WALL | MAP_MASK_WALK | SET_CELL_TYPE_ID(WALL_TYPE_ARCH));

	case 'D': // Unlocked Door
		return (MAP_MASK_WALL | MAP_MASK_WALK | SET_CELL_TYPE_ID(WALL_TYPE_UNLOCKED_DOOR));

	case 'T': // Locked Door
		return (MAP_MASK_WALL | SET_CELL_TYPE_ID(WALL_TYPE_LOCKED_DOOR));

	case 'S': // Shootable wall. Seen through, so the ray runs on and the wall style draws what is behind it as the tell.
		return (MAP_MASK_WALL | SET_CELL_TYPE_ID(WALL_TYPE_SHOOTABLE));

	case 'B': // Iron Bars
		return (MAP_MASK_WALL | SET_CELL_TYPE_ID(WALL_TYPE_BARS));

	case 'V': // The void
		return (MAP_MASK_WALL | MAP_MASK_SOLID | SET_CELL_TYPE_ID(WALL_TYPE_VOID));

	case '!': // Sign or wall mounted feature
		return (MAP_MASK_WALL | MAP_MASK_SOLID | SET_CELL_TYPE_ID(WALL_TYPE_SIGN));

	case '*': // Light fitting
		return (MAP_MASK_WALL | MAP_MASK_SOLID | SET_CELL_TYPE_ID(WALL_TYPE_LIGHT));

	case ':': // Pipe and cable runs
		return (MAP_MASK_WALL | MAP_MASK_SOLID | SET_CELL_TYPE_ID(WALL_TYPE_PIPES));

	case '#': // Dado: a wall finished differently below than above. Tiles in the lab style, shelving in the default
		return (MAP_MASK_WALL | MAP_MASK_SOLID | SET_CELL_TYPE_ID(WALL_TYPE_DADO));

	// Not solid, so the ray runs on past these two and draws what is behind
	// them first.
	case 'R': // Low wall. Seen over, but not walked through.
		return (MAP_MASK_WALL | SET_CELL_TYPE_ID(WALL_TYPE_LOW));

	case '|': // Pillar. Seen past on both sides.
		return (MAP_MASK_WALL | SET_CELL_TYPE_ID(WALL_TYPE_PILLAR));

	case 'U': // Switch. Thrown by shooting it.
		return (MAP_MASK_WALL | MAP_MASK_SOLID | SET_CELL_TYPE_ID(WALL_TYPE_SWITCH));

	case 'C':
	case 'E':
	case 'F':
	case 'G':
		return getEnemyCell(x, y, cell);



	case 'H':
	case 'I':
	case 'J':
	case 'K':
	case 'L':
	case 'M':
	case 'N':
	case 'O':
		return getPickupCell(x, y, cell);

	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
		return getDecorCell(x, y, cell);

	}

	// Unknown type. Return void.
	return (MAP_MASK_WALL | MAP_MASK_SOLID | SET_CELL_TYPE_ID(7));
}

/* ------------------------------------------------------------ map file */

/* The stage buffer. A [MAP] row and a [LEVEL] line must fit in it; the text
   sections stream through it a chunk at a time, so a briefing paragraph typed
   as one long line in an editor costs nothing extra. */
#define MAP_LINE_MAX 80

/* Far storage for the level text: title, location, briefing and objectives,
   each a NUL terminated string, back to back. 192 paragraphs is 3072 bytes,
   comfortably over a 200 word briefing plus five objectives, and none of it
   is near data. */
#define MAP_TEXT_PARAS 192
#define MAP_TEXT_BYTES (MAP_TEXT_PARAS * 16)

/* Where the player spawns when a file has no [LEVEL] section: the map 1
   corridor, facing east, which is what initPlayer() used to hardcode. */
#define MAP_DEFAULT_START_X 27
#define MAP_DEFAULT_START_Y 1

#define SECTION_MAP 0
#define SECTION_LEVEL 1
#define SECTION_BRIEFING 2
#define SECTION_OBJECTIVE 3
#define SECTION_HEADER 4	/* reading a [NAME] line */

typedef struct parser_t
{
	char line[MAP_LINE_MAX];
	u8 lineLen;
	u8 section;
	u8 lineStart;	/* the next byte is the first on its line */
	u8 skipLine;	/* a ; comment: discard to the end of the line */
	u8 lineHadText;	/* something other than whitespace seen on this line */
	u8 pendingSpaces;	/* whitespace held back until a word follows it */
	u8 hadText;	/* a word has been written for the current text item */
	u8 pendingBreak;	/* a blank line separated it from the next word */
	u8 titleDone;	/* [OBJECTIVE]: the first line has been closed off */
	u8 mapDone;	/* a complete 64x64 grid has been read */
	u16 lineNo;
	u16 x, y;	/* [MAP] cursor */
} parser_t;

static HANDLE mapTextSeg = 0;

/* The reason a load failed is only worth the string on the PC, where there is
   a stderr to print it to. On the device the literals would land in DGROUP. */
#ifdef PSION3D_PC
#include <stdio.h>
static u16 mapFail(const parser_t *ps, const char *why)
{
	fprintf(stderr, "loadMap: line %u: %s\n", (unsigned)ps->lineNo, why);
	return FALSE;
}
#define MAP_FAIL(ps, why) mapFail((ps), (why))
#else
#define MAP_FAIL(ps, why) FALSE
#endif

static void resetMapInfo(void)
{
	u8 i;

	mapInfo.startX = MAP_DEFAULT_START_X;
	mapInfo.startY = MAP_DEFAULT_START_Y;
	mapInfo.f_startAngle = 0;
	mapInfo.endX = 0;
	mapInfo.endY = 0;
	mapInfo.mapPosX = 0;
	mapInfo.mapPosY = 0;
	mapInfo.objectiveCount = 0;
	mapInfo.stationCount = 0;
	mapInfo.titleOfs = MAP_TEXT_NONE;
	mapInfo.locationOfs = MAP_TEXT_NONE;
	mapInfo.briefingOfs = MAP_TEXT_NONE;
	mapInfo.textLen = 0;

	for (i = 0; i < MAP_MAX_OBJECTIVES; i++)
	{
		mapInfo.objectiveOfs[i] = MAP_TEXT_NONE;
		mapInfo.objectiveBriefOfs[i] = MAP_TEXT_NONE;
	}
}

/* Append n bytes to the text segment. FALSE when the cap is reached. */
static u16 textAppend(const char *s, const u16 n)
{
	if (n == 0)
		return TRUE;

	if ((u32)mapInfo.textLen + n > MAP_TEXT_BYTES)
		return FALSE;

	p_sgcopyto(mapTextSeg, (long)mapInfo.textLen, (VOID *)s, n);
	mapInfo.textLen += n;

	return TRUE;
}

static u16 textAppendChar(const char c)
{
	return textAppend(&c, 1);
}

/* Append a whole string with its terminator and return where it starts. */
static u16 textAppendString(const char *s, u16 *ofs)
{
	u16 n = 0;

	while (s[n])
		n++;

	*ofs = mapInfo.textLen;

	return textAppend(s, (u16)(n + 1));
}

static u16 isSpace(const char c)
{
	return c == ' ' || c == '\t';
}

static char lower(const char c)
{
	return (c >= 'A' && c <= 'Z') ? (char)(c + ('a' - 'A')) : c;
}

/* Case-insensitive match of a keyword against the start of s, which must end
   there or in whitespace. */
static u16 keyIs(const char *s, const char *key)
{
	while (*key)
	{
		if (lower(*s) != *key)
			return FALSE;

		s++;
		key++;
	}

	return *s == 0 || isSpace(*s);
}

/* Read an unsigned decimal at *p, skipping leading spaces, and step past it. */
static u16 parseNum(const char **p, u16 *out)
{
	const char *s = *p;
	u16 v = 0;

	while (isSpace(*s))
		s++;

	if (*s < '0' || *s > '9')
		return FALSE;

	while (*s >= '0' && *s <= '9')
	{
		v = (u16)(v * 10 + (*s - '0'));
		s++;
	}

	*p = s;
	*out = v;

	return TRUE;
}

/* A comma at *p, spaces before it allowed, and step past it. */
static u16 parseComma(const char **p)
{
	const char *s = *p;

	while (isSpace(*s))
		s++;

	if (*s != ',')
		return FALSE;

	*p = s + 1;

	return TRUE;
}

/* "x, y" and nothing after it. */
static u16 parsePair(const char *s, u16 *a, u16 *b)
{
	if (!parseNum(&s, a) || !parseComma(&s) || !parseNum(&s, b))
		return FALSE;

	while (isSpace(*s))
		s++;

	return *s == 0;
}

/* A compass bearing in degrees to the engine's angle. The file holds 0 north
   (up the grid) and 90 east. The engine's 0 is +x, east, and it turns towards
   +y, south, so the bearing runs a quarter turn ahead of it. Then degrees to
   Q8 radians: a full turn is 1608 (2 pi * 256), and 1608 / 360 is exactly
   67 / 15. 359 * 67 fits a u16. */
static f16 bearingToAngle(u16 bearing)
{
	bearing = (u16)((bearing + 270) % 360);

	return (f16)(bearing * 67 / 15);
}

/* Flush what the stage buffer holds of the current text line. */
static u16 textFlush(parser_t *ps)
{
	u16 ok = textAppend(&ps->line[0], ps->lineLen);

	ps->lineLen = 0;

	return ok;
}

static void beginLine(parser_t *ps)
{
	ps->lineStart = TRUE;
	ps->skipLine = FALSE;
	ps->lineHadText = FALSE;
	ps->pendingSpaces = 0;
	ps->lineLen = 0;
	ps->lineNo++;
}

/* A text line has ended (newline or EOF). Lines are joined with one space and
   a blank line becomes a paragraph break, so the author's wrapping does not
   matter. In an [OBJECTIVE] the first line is the title, closed off here. */
static u16 endTextLine(parser_t *ps)
{
	if (!textFlush(ps))
		return MAP_FAIL(ps, "level text exceeds MAP_TEXT_BYTES");

	if (ps->lineHadText)
	{
		ps->hadText = TRUE;

		if (ps->section == SECTION_OBJECTIVE && !ps->titleDone)
		{
			if (!textAppendChar(0))
				return MAP_FAIL(ps, "level text exceeds MAP_TEXT_BYTES");

			ps->titleDone = TRUE;
			ps->hadText = FALSE;
			mapInfo.objectiveBriefOfs[mapInfo.objectiveCount - 1] = mapInfo.textLen;
		}
	}
	else if (ps->hadText)
	{
		ps->pendingBreak = TRUE;
	}

	return TRUE;
}

/* One byte of a text section. Leading and trailing whitespace on a line is
   dropped; the bytes between stream to the segment through the stage buffer. */
static u16 textByte(parser_t *ps, const char c)
{
	if (isSpace(c))
	{
		if (ps->lineHadText)
			ps->pendingSpaces++;

		return TRUE;
	}

	if (!ps->lineHadText)
	{
		ps->lineHadText = TRUE;

		if (ps->pendingBreak)
		{
			if (!textAppendChar('\n'))
				return MAP_FAIL(ps, "level text exceeds MAP_TEXT_BYTES");

			ps->pendingBreak = FALSE;
		}
		else if (ps->hadText)
		{
			if (!textAppendChar(' '))
				return MAP_FAIL(ps, "level text exceeds MAP_TEXT_BYTES");
		}
	}

	while (ps->pendingSpaces)
	{
		ps->line[ps->lineLen++] = ' ';
		ps->pendingSpaces--;

		if (ps->lineLen == MAP_LINE_MAX && !textFlush(ps))
			return MAP_FAIL(ps, "level text exceeds MAP_TEXT_BYTES");
	}

	ps->line[ps->lineLen++] = c;

	if (ps->lineLen == MAP_LINE_MAX && !textFlush(ps))
		return MAP_FAIL(ps, "level text exceeds MAP_TEXT_BYTES");

	return TRUE;
}

/* A complete "key = value" line from [LEVEL]. */
static u16 levelLine(parser_t *ps)
{
	char *s = &ps->line[0];
	char *eq;
	char *end;
	u16 a, b;

	ps->line[ps->lineLen] = 0;
	ps->lineLen = 0;

	while (isSpace(*s))
		s++;

	if (*s == 0 || *s == ';')
		return TRUE;

	/* Trailing whitespace, and whatever preceded a ; comment. */
	for (end = s; *end; end++)
		;

	while (end > s && isSpace(end[-1]))
		end--;

	*end = 0;

	for (eq = s; *eq && *eq != '='; eq++)
		;

	if (*eq != '=')
		return MAP_FAIL(ps, "[LEVEL] line is not key = value");

	*eq = 0;
	eq++;

	while (isSpace(*eq))
		eq++;

	if (keyIs(s, "start"))
	{
		if (!parsePair(eq, &a, &b) || a >= MAP_X || b >= MAP_Y)
			return MAP_FAIL(ps, "start wants x, y inside the map");

		mapInfo.startX = (u8)a;
		mapInfo.startY = (u8)b;
	}
	else if (keyIs(s, "end"))
	{
		if (!parsePair(eq, &a, &b) || a >= MAP_X || b >= MAP_Y)
			return MAP_FAIL(ps, "end wants x, y inside the map");

		mapInfo.endX = (u8)a;
		mapInfo.endY = (u8)b;
	}
	else if (keyIs(s, "mappos"))
	{
		if (!parsePair(eq, &a, &b))
			return MAP_FAIL(ps, "mappos wants x, y");

		mapInfo.mapPosX = a;
		mapInfo.mapPosY = b;
	}
	else if (keyIs(s, "angle"))
	{
		const char *p = eq;

		if (!parseNum(&p, &a))
			return MAP_FAIL(ps, "angle wants a bearing in degrees");

		mapInfo.f_startAngle = bearingToAngle(a);
	}
	else if (keyIs(s, "station"))
	{
		/* x, y, bearing, then the rest of the line is the name. */
		const char *p = eq;
		station_t *st;
		u16 bearing;

		if (mapInfo.stationCount >= MAP_MAX_STATIONS)
			return MAP_FAIL(ps, "more than MAP_MAX_STATIONS station lines");

		if (!parseNum(&p, &a) || !parseComma(&p) || !parseNum(&p, &b) ||
			!parseComma(&p) || !parseNum(&p, &bearing) || !parseComma(&p) ||
			a >= MAP_X || b >= MAP_Y)
			return MAP_FAIL(ps, "station wants x, y, bearing, name");

		while (isSpace(*p))
			p++;

		if (*p == 0)
			return MAP_FAIL(ps, "station has no name");

		st = &mapInfo.stations[mapInfo.stationCount];
		st->x = (u8)a;
		st->y = (u8)b;
		st->f_angle = bearingToAngle(bearing);

		if (!textAppendString(p, &st->nameOfs))
			return MAP_FAIL(ps, "level text exceeds MAP_TEXT_BYTES");

		mapInfo.stationCount++;
	}
	else if (keyIs(s, "title"))
	{
		if (!textAppendString(eq, &mapInfo.titleOfs))
			return MAP_FAIL(ps, "level text exceeds MAP_TEXT_BYTES");
	}
	else if (keyIs(s, "location"))
	{
		if (!textAppendString(eq, &mapInfo.locationOfs))
			return MAP_FAIL(ps, "level text exceeds MAP_TEXT_BYTES");
	}
	else
	{
		return MAP_FAIL(ps, "unknown [LEVEL] key");
	}

	return TRUE;
}

/* The section in progress has ended: a new header, or the end of the file. */
static u16 closeSection(parser_t *ps)
{
	switch (ps->section)
	{
	case SECTION_MAP:
		if (ps->x == 0 && ps->y == 0)
			return TRUE;	/* nothing between the file start and [MAP] */

		if (ps->y != MAP_Y || ps->x != 0)
			return MAP_FAIL(ps, "map grid is not 64 x 64");

		ps->mapDone = TRUE;
		return TRUE;

	case SECTION_LEVEL:
		return ps->lineLen ? levelLine(ps) : TRUE;

	case SECTION_BRIEFING:
		if (!endTextLine(ps))
			return FALSE;

		return textAppendChar(0) ? TRUE : MAP_FAIL(ps, "level text exceeds MAP_TEXT_BYTES");

	case SECTION_OBJECTIVE:
		if (!endTextLine(ps))
			return FALSE;

		if (!ps->titleDone)
			return MAP_FAIL(ps, "[OBJECTIVE] has no title line");

		return textAppendChar(0) ? TRUE : MAP_FAIL(ps, "level text exceeds MAP_TEXT_BYTES");
	}

	return TRUE;
}

/* A complete [NAME] line. The section before it was closed at the [. */
static u16 openSection(parser_t *ps)
{
	char *s = &ps->line[1];
	char *end;

	ps->line[ps->lineLen] = 0;
	ps->lineLen = 0;

	for (end = s; *end && *end != ']'; end++)
		;

	if (*end != ']')
		return MAP_FAIL(ps, "section header has no ]");

	*end = 0;

	ps->hadText = FALSE;
	ps->pendingBreak = FALSE;

	if (keyIs(s, "map"))
	{
		if (ps->mapDone)
			return MAP_FAIL(ps, "second [MAP] section");

		ps->section = SECTION_MAP;
	}
	else if (keyIs(s, "level"))
	{
		ps->section = SECTION_LEVEL;
	}
	else if (keyIs(s, "briefing"))
	{
		ps->section = SECTION_BRIEFING;
		mapInfo.briefingOfs = mapInfo.textLen;
	}
	else if (keyIs(s, "objective"))
	{
		if (mapInfo.objectiveCount >= MAP_MAX_OBJECTIVES)
			return MAP_FAIL(ps, "more than MAP_MAX_OBJECTIVES [OBJECTIVE] sections");

		ps->section = SECTION_OBJECTIVE;
		ps->titleDone = FALSE;
		mapInfo.objectiveOfs[mapInfo.objectiveCount] = mapInfo.textLen;
		mapInfo.objectiveCount++;
	}
	else
	{
		return MAP_FAIL(ps, "unknown section");
	}

	return TRUE;
}

/* One byte of the file. */
static u16 feed(parser_t *ps, const char c)
{
	if (c == '\r')
		return TRUE;

	if (c == '\n')
	{
		u16 ok = TRUE;

		switch (ps->section)
		{
		case SECTION_HEADER:
			ok = openSection(ps);
			break;

		case SECTION_LEVEL:
			ok = levelLine(ps);
			break;

		case SECTION_BRIEFING:
		case SECTION_OBJECTIVE:
			ok = endTextLine(ps);
			break;
		}

		beginLine(ps);
		return ok;
	}

	if (ps->lineStart)
	{
		ps->lineStart = FALSE;

		if (c == '[')
		{
			/* Close the section in progress now, while its state is intact,
			   then read the header into the stage buffer. */
			if (!closeSection(ps))
				return FALSE;

			ps->section = SECTION_HEADER;
			ps->line[0] = c;
			ps->lineLen = 1;
			return TRUE;
		}

		/* A ; line is a comment everywhere, the grid included: it is not a
		   cell character, and a valid grid is at a row boundary here. */
		if (c == ';')
			ps->skipLine = TRUE;
	}

	if (ps->skipLine)
		return TRUE;

	switch (ps->section)
	{
	case SECTION_MAP:
		if (ps->y >= MAP_Y)
			return MAP_FAIL(ps, "map grid has more than 64 rows");

		map[ps->y][ps->x] = getCellEncoding(ps->x, ps->y, (s8)c);
		ps->x++;

		if (ps->x >= MAP_X)
		{
			ps->x = 0;
			ps->y++;
		}

		return TRUE;

	case SECTION_HEADER:
	case SECTION_LEVEL:
		/* A ; after the value starts a comment, which never reaches the stage
		   buffer, so it can run as long as the author likes. */
		if (c == ';')
		{
			ps->skipLine = TRUE;
			return TRUE;
		}

		if (ps->lineLen >= MAP_LINE_MAX - 1)
			return MAP_FAIL(ps, "line is longer than MAP_LINE_MAX");

		ps->line[ps->lineLen++] = c;
		return TRUE;

	case SECTION_BRIEFING:
	case SECTION_OBJECTIVE:
		return textByte(ps, c);
	}

	return TRUE;
}

u16 loadMapFile(const u8 mapId)
{
	TEXT fileName[MAP_FILE_NAME_LEN];
	VOID *fileHandle;
	parser_t ps;
	INT bytesRead;
	INT i;
	char chunk[64];

	resetEnemy();
	resetMapInfo();

	if (mapTextSeg > 0)
		p_sgclose(mapTextSeg);

	mapTextSeg = p_sgcreate("MAPTXT", MAP_TEXT_PARAS, E_SEGMENT_HIGH);

	if (mapTextSeg <= 0)
		return FALSE;

	p_atos(&fileName[0], "LOC::M:\\IMG\\MAP\\map%d.map", mapId);

	if (p_open(&fileHandle, &fileName[0], P_FOPEN | P_FSTREAM) != 0)
		return FALSE;

	/* A file that starts straight in on the grid, as the originals did, is
	   read as if it opened with [MAP]. */
	ps.section = SECTION_MAP;
	ps.hadText = FALSE;
	ps.pendingBreak = FALSE;
	ps.titleDone = FALSE;
	ps.mapDone = FALSE;
	ps.lineNo = 0;
	ps.x = 0;
	ps.y = 0;
	beginLine(&ps);

	/* The parser takes a byte at a time; the file arrives in chunks because
	   the mission index parses every level file at startup, and a p_read per
	   byte is what made a load slow. A short final chunk is normal; the end
	   proper is E_FILE_EOF on the read after it. */
	while (TRUE)
	{
		bytesRead = p_read(fileHandle, &chunk[0], sizeof(chunk));

		if (bytesRead == E_FILE_EOF)
			break;

		if (bytesRead <= 0)
		{
			p_close(fileHandle);
			return FALSE;
		}

		for (i = 0; i < bytesRead; i++)
		{
			if (!feed(&ps, chunk[i]))
			{
				p_close(fileHandle);
				return FALSE;
			}
		}
	}

	p_close(fileHandle);

	/* The last line may have no newline behind it. */
	if (ps.section == SECTION_HEADER)
		return MAP_FAIL(&ps, "section header at end of file");

	if (!closeSection(&ps))
		return FALSE;

	if (!ps.mapDone)
		return MAP_FAIL(&ps, "no 64 x 64 map grid");

	return TRUE;
}

u16 loadMap(const u8 mapId)
{
	if (!loadMapFile(mapId))
		return FALSE;

	loadMapData(mapId);

	return TRUE;
}

u16 mapTextCopy(const u16 ofs, char *buf, const u16 bufLen)
{
	u16 n, i;

	if (bufLen == 0)
		return 0;

	if (ofs == MAP_TEXT_NONE || mapTextSeg <= 0 || ofs >= mapInfo.textLen)
	{
		buf[0] = 0;
		return 0;
	}

	n = (u16)(mapInfo.textLen - ofs);

	if (n > bufLen - 1)
		n = (u16)(bufLen - 1);

	p_sgcopyfr(mapTextSeg, (long)ofs, buf, n);
	buf[n] = 0;

	for (i = 0; i < n && buf[i]; i++)
		;

	return i;
}

/* Open the locked door at x, y, if that is what the cell is. The keycard is
   not checked at the door and neither is the switch: the cell is rewritten to
   the unlocked encoding, which is what 'D' produces, so both the MAP_MASK_WALK
   that lets the player through and the open-on-approach door drawing follow
   with no further test in the hot paths. Anything else is left alone, so a
   level script can name a cell without checking it first. */
void unlockDoor(const u16 x, const u16 y)
{
	const u16 cell = mapCell(x, y);

	/* Enemy cells carry type values 0..3 as well, so the wall bit is what
	   makes this a door rather than a sprite. */
	if (!isWall(cell) || mapCellType(cell) != WALL_TYPE_LOCKED_DOOR)
		return;

	updateCell(x, y, (MAP_MASK_WALL | MAP_MASK_WALK | SET_CELL_TYPE_ID(WALL_TYPE_UNLOCKED_DOOR)));
}

/* Every locked door on the level: the default for a keycard or a switch on a
   level whose script does not say otherwise. */
void unlockDoors(void)
{
	u16 x, y;

	for (y = 0; y < MAP_Y; y++)
	{
		for (x = 0; x < MAP_X; x++)
			unlockDoor(x, y);
	}
}
