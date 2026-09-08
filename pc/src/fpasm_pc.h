#ifndef FPASM_PC_H
#define FPASM_PC_H

#ifdef __cplusplus
extern "C" {
#endif

/* Non-zero when fpsplit_t correctly overlays s32, which fpdiv depends on. */
int fpLayoutOk(void);

#ifdef __cplusplus
}
#endif

#endif
