/*  plib.h - PC replacement for the SIBO PLIB header.

    This file is never seen by the Psion build. The native build simply leaves
    the real SDK include directory off the compiler's search path and puts
    pc/compat/ on it instead, so fp_types.h's `#include <plib.h>` resolves
    here. That is why not one line of the game sources had to change.

    Declarations are copied from E:\Dosroot\SIBOSDK\include (p_std.h, p_file.h,
    p_graf.h, epoc.h) so they can be diffed against the originals. The one
    deliberate difference is width: the SDK spells INT/UINT/HANDLE as `int`,
    which is 16 bits under TopSpeed, so they are `short` here. Every SDK-typed
    variable in the game then behaves at device width for free. */

#ifndef PLIB_H
#define PLIB_H

/* ---- p_std.h ---- */

#define GLREF_D extern
#define GLDEF_D
#define LOCAL_D static
#define GLREF_C extern
#define LOCAL_C static
#define GLDEF_C
#define FOREVER for(;;)
#define VOID void
#define FAST
#define CONST const

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif
#ifndef NULL
#define NULL 0
#endif
#define NULL_D ((VOID *)0)

typedef short int          INT;     /* `int` on the SDK: 16 bits on TopSpeed. */
typedef short int          HANDLE;
typedef unsigned short int UINT;
typedef signed char        BYTE;
typedef unsigned char      UBYTE;
typedef char               TEXT;
typedef short int          WORD;
typedef unsigned short int UWORD;
typedef long int           LONG;
typedef unsigned long int  ULONG;
typedef double             DOUBLE;
typedef float              FLOAT;

/* ---- p_file.h ---- */

#define P_FOPEN        0x0000
#define P_FSTREAM      0x0000
#define P_FSTREAM_TEXT 0x0010

#define EofErr     (-36)
#define E_FILE_EOF EofErr

/* ---- epoc.h ---- */

#define E_SEGMENT_HIGH 1

/* ---- p_graf.h ---- */

typedef struct { short int x, y; } P_POINT;
typedef struct { P_POINT tl, br; } P_RECT;

/* ---- functions, implemented in pc/src/plib_pc.c ---- */

#ifdef __cplusplus
extern "C" {
#endif

extern INT   p_open (VOID **, const TEXT *, UINT);
extern INT   p_read (VOID *, VOID *, UINT);
extern INT   p_close(VOID *);

/* `cdecl` dropped: it is a TopSpeed keyword and the default on PC anyway. */
extern void  p_atos(TEXT *, const TEXT *, ...);
extern UINT  p_slen(const TEXT *);

extern UINT  p_returntickcount(void);

extern HANDLE p_sgcreate(const TEXT *, INT, INT);
extern HANDLE p_sgopen  (const TEXT *);
extern INT    p_sgclose (HANDLE);
extern INT    p_sgcopyto(HANDLE, long int, VOID *, UINT);
extern INT    p_sgcopyfr(HANDLE, long int, VOID *, UINT);

#ifdef __cplusplus
}
#endif

#endif
