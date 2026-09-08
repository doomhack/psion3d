#ifndef DEBUG_H
#define DEBUG_H

#include <plib.h>
#include "fp_types.h"

/*  Project types rather than the SDK's TEXT and INT, for the same reason as
    sprite.h: they are identical under TopSpeed, and using them here keeps
    <plib.h> out of every file that wants a debug slot. */
void setDbgInt(s32 dbg);
void setDbgFp(f16 dbg);
void setDbgString(TEXT *dbg);
void drawDbgText(INT x, INT y);

#endif
