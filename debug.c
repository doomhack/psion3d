#include "debug.h"

#include <wlib.h>

static TEXT dbgTxt[32];

void setDbgInt(s32 dbg)
{
	p_atos(&dbgTxt[0], "%d", dbg);
}

void setDbgFp(f16 dbg)
{
	DOUBLE ddbg, div;
	s16 idiv = 256;
	P_DTOB dtob;

	dtob.type = P_DTOB_GENERAL;
	dtob.width = 31;
	dtob.ndec = 0;
	dtob.point = ".";
	dtob.triad = ",";
	dtob.trilen = 0;

	p_itof(&ddbg, &dbg);
	p_itof(&div, &idiv);

	p_fdiv(&ddbg, &div);

	p_dtob(&dbgTxt[0], &ddbg, &dtob);
}

void setDbgString(TEXT *dbg)
{
	INT i = 0;

	while(i < 31 && dbg[i] != '\0')
	{
		dbgTxt[i] = dbg[i];
		i++;
	}

	dbgTxt[i] = '\0';
}

void drawDbgText(INT x, INT y)
{
	gPrintText(x, y, &dbgTxt[0], p_slen(&dbgTxt[0]));
}