#ifndef DEBUG_PC_H
#define DEBUG_PC_H

#ifdef __cplusplus
extern "C" {
#endif

/* The string most recently set by setDbgInt / setDbgFp / setDbgString. */
const char *pcDebugText(void);

#ifdef __cplusplus
}
#endif

#endif
