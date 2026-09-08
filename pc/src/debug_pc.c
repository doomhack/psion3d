/*  debug_pc.c - PC replacement for debug.c.

    debug.c itself is not in the PC build: it renders through gPrintText into
    the Psion's separate 120x160 debug window, and its setDbgFp path is the
    only floating point in the program, done with PLIB's software float
    package (p_itof / p_fdiv / p_dtob). Here the same four entry points just
    maintain a string that the Qt HUD reads. */

#include "debug.h"
#include "debug_pc.h"

#include <stdio.h>

static char dbgTxt[32];

void setDbgInt(s32 dbg)
{
	/* Note this is a genuine latent bug on the device: s32 is 4 bytes but
	   p_atos's "%d" consumes 2 in 16-bit cdecl varargs. Harmless there only
	   because setDbgInt currently has no callers. */
	snprintf(dbgTxt, sizeof(dbgTxt), "%ld", (long)dbg);
}

void setDbgFp(f16 dbg)
{
	/* Q8 as a decimal, without dragging in a float package. */
	s32 whole = dbg >> FP_BITS;
	u16 frac  = (u16)(((u16)(dbg < 0 ? -dbg : dbg) & 0xFF) * 1000u / 256u);

	snprintf(dbgTxt, sizeof(dbgTxt), "%s%ld.%03u",
	         (dbg < 0 && whole == 0) ? "-" : "", (long)whole, (unsigned)frac);
}

void setDbgString(char *dbg)
{
	s16 i = 0;

	while(i < 31 && dbg[i] != '\0')
	{
		dbgTxt[i] = dbg[i];
		i++;
	}

	dbgTxt[i] = '\0';
}

void drawDbgText(s16 x, s16 y)
{
	/* The Qt HUD lays the debug slot out itself; the device's coordinates do
	   not apply. Kept so the signature in debug.h stays satisfied. */
	(void)x;
	(void)y;
}

const char *pcDebugText(void)
{
	return dbgTxt;
}
