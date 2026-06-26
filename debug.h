#ifndef DEBUG_H
#define DEBUG_H

#include "fp_types.h"

void setDbgInt(s32 dbg);
void setDbgFp(f16 dbg);
void setDbgString(TEXT *dbg);
void drawDbgText(INT x, INT y);

#endif
