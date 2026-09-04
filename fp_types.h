#ifndef FP_TYPES_H
#define FP_TYPES_H

#include <plib.h>

typedef unsigned char u8;
typedef signed char s8;

typedef unsigned short u16;
typedef signed short s16;

typedef unsigned long u32;
typedef signed long s32;


//Fixed point math.

#define FP_MAX  ((f16)32767)
#define FP_MIN  ((f16)-32768)

#define FP_BITS 8
typedef s16 f16;

#define flt2fp(x) ((f16)((x) * (1 << FP_BITS)))
#define fp2int(x) ((s16)((x) >> FP_BITS))
#define int2fp(x) ((f16)((x) << FP_BITS))


/* The TopSpeed runtime implements 32 bit shifts as a called shift-by-one loop
   (N$LngShl / N$LngShr), so a >> FP_BITS on an s32 costs a CALL plus eight
   iterations. Splitting the value into words lets the compiler do the same job
   with plain 16 bit register moves. Both helpers below assume FP_BITS == 8. */
typedef union
{
	s32 l;
	struct { u16 lo; s16 hi; } w;	/* x86 is little endian. */
} fpsplit_t;

static f16 fpmul(const f16 a, const f16 b)
{
	fpsplit_t r;

	r.l = (s32)a * b;

	/* Bits 8..23 of the product, which is (r.l >> FP_BITS) truncated to f16. */
	return (f16)((r.w.lo >> FP_BITS) | (r.w.hi << FP_BITS));
}

LOCAL_C f16 fpdiv(const f16 a, const f16 b)
{
    s32 aa;
    fpsplit_t n;

    if(b == 0)
        return a >= 0 ? FP_MAX : FP_MIN;

    /* Build (s32)a << FP_BITS directly rather than calling N$LngShl. */
    n.w.lo = (u16)(((u16)a) << FP_BITS);
    n.w.hi = (s16)(a >> FP_BITS);

    aa = n.l / b;

    if(aa > FP_MAX)
        return FP_MAX;

    if(aa < FP_MIN)
        return FP_MIN;

    return (f16)aa;
}

#endif