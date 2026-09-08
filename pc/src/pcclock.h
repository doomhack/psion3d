#ifndef PSION3D_PCCLOCK_H
#define PSION3D_PCCLOCK_H

#ifdef __cplusplus
extern "C" {
#endif

/*  Monotonic wall clock in milliseconds.

    This lives in its own translation unit because <windows.h> typedefs INT,
    UINT, BYTE and WORD, and so does the SIBO compat header in pc/compat. The
    two sets disagree - the SDK's are 16-bit - so no file may include both.
    Keeping the clock separate is what lets plib_pc.c speak SIBO types. */
double pcMonotonicMs(void);

#ifdef __cplusplus
}
#endif

#endif
