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


static f16 fpmul(f16 a, f16 b)
{
	s32 aa = a;
	
	return (aa * b) >> FP_BITS;
}

LOCAL_C f16 fpdiv(f16 a, f16 b)
{
    s32 aa = a;

    if(b == 0)
        return a >= 0 ? FP_MAX : FP_MIN;

    aa = (aa << FP_BITS) / b;

    if(aa > FP_MAX)
        return FP_MAX;

    if(aa < FP_MIN)
        return FP_MIN;

    return (f16)aa;
}

#endif