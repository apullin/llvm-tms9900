#ifndef _MATH_H
#define _MATH_H
#define INFINITY __builtin_inff()
#define NAN __builtin_nanf("")
#define HUGE_VALF __builtin_huge_valf()
#define signbit(x) __builtin_signbit(x)
#define isnan(x) __builtin_isnan(x)
#define isinf(x) __builtin_isinf(x)
#define isfinite(x) __builtin_isfinite(x)
#define fabs(x) __builtin_fabs(x)
#define fabsf(x) __builtin_fabsf(x)
float fmaxf(float a, float b);
#endif
