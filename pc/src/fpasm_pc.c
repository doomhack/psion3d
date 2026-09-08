/*  fpasm_pc.c - portable replacement for fpasm.a.

    The device implements fpmul in assembler because C cannot reach the V30's
    native IMUL: (s32)a * b promotes both operands and TopSpeed emits its
    generic 32x32 helper N$SgnMol instead. On a PC that concern evaporates -
    the expression below compiles to a single imul. */

#include "fp_types.h"
#include "fpasm_pc.h"

/*  C89-safe static assertions. fp_types.h types s32 as `long`, which is 32
    bits under MSVC and MinGW-w64 but 64 bits on LP64 (Linux, macOS). If that
    ever changes, fpsplit_t stops overlaying s32 and fpdiv silently returns
    garbage with no diagnostic at all - so fail the build here instead. */
typedef char psion3d_s32_is_32_bits[(sizeof(s32) == 4) ? 1 : -1];
typedef char psion3d_f16_is_16_bits[(sizeof(f16) == 2) ? 1 : -1];

/*  Same bits as the assembler: the signed 16x16 -> 32 product's 8..23,
    truncated to 16. There is deliberately NO clamp - sprite.c:364-373 rejects
    a projection precisely because this wraps, and draw.c:341-353 relies on it
    too. Adding saturation would change game behaviour, not just fix an edge
    case. */
f16 fpmul(const f16 a, const f16 b)
{
	return (f16)(((s32)a * (s32)b) >> FP_BITS);
}

/*  fpsplit_t assumes a little-endian s32. Checked at startup rather than left
    to produce quietly wrong fpdiv results on some future host. */
int fpLayoutOk(void)
{
	fpsplit_t t;

	t.l = 0x00010002L;

	return t.w.lo == 0x0002 && t.w.hi == 0x0001;
}
