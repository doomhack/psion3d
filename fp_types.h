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

/* Written as -32767 - 1 rather than -32768 because int is 16 bits here: the
   literal 32768 does not fit, so the compiler widens it to long and warns at
   every use. Both halves of this form stay in int, and it is the same value.
   This is how limits.h spells INT_MIN, for the same reason. */
#define FP_MIN  ((f16)(-32767 - 1))

#define FP_BITS 8
typedef s16 f16;

#define flt2fp(x) ((f16)((x) * (1 << FP_BITS)))
#define fp2int(x) ((s16)((x) >> FP_BITS))
#define int2fp(x) ((f16)((x) << FP_BITS))


/* The TopSpeed runtime implements 32 bit shifts as a called shift-by-one loop
   (N$LngShl / N$LngShr), so a << FP_BITS on an s32 costs a CALL plus eight
   iterations. Splitting the value into words lets fpdiv build the same value
   with plain 16 bit register moves. Assumes FP_BITS == 8. */
typedef union
{
	s32 l;
	struct { u16 lo; s16 hi; } w;	/* x86 is little endian. */
} fpsplit_t;

/* Implemented in fpasm.a. The V30 multiplies 16x16 into 32 bits natively in
   about 25 cycles, but C cannot reach that instruction: (s32)a * b promotes
   both operands, so TopSpeed emits the generic 32x32 helper N$SgnMol instead.
   Measured at roughly 190 calls per frame in the ray cast alone, which made it
   the single most expensive operation in the program. The assembler version
   returns the same bits, the product's 8..23.

   The convention is declared rather than assumed: a arrives in AX, b in BX, the
   result comes back in AX, and the routine touches nothing else. Saying so also
   stops the compiler spilling registers around every call. */
#pragma save, call(reg_param=>(ax,bx), reg_saved=>(bx,cx,dx,si,di,es,ds,st1,st2))
f16 fpmul(const f16 a, const f16 b);
#pragma restore

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