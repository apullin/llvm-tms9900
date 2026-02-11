// Minimal math.h for TMS9900 freestanding libc++
#ifndef _TMS9900_MATH_H
#define _TMS9900_MATH_H
#define _LIBCPP_MATH_H  // satisfy libc++ cmath check

#define FP_NAN       0
#define FP_INFINITE  1
#define FP_ZERO      2
#define FP_NORMAL    3
#define FP_SUBNORMAL 4

#define HUGE_VAL     __builtin_huge_val()
#define HUGE_VALF    __builtin_huge_valf()
#define INFINITY     __builtin_inff()
#define NAN          __builtin_nanf("")


#ifdef __cplusplus
extern "C" {
#endif

int __fpclassifyf(float __x);
int __fpclassifyd(double __x);

#ifdef __cplusplus
} // extern "C"

// C++ inline wrappers for FP classification (required by libc++ cmath)
inline int fpclassify(float __x) { return __builtin_fpclassify(FP_NAN, FP_INFINITE, FP_NORMAL, FP_SUBNORMAL, FP_ZERO, __x); }
inline int fpclassify(double __x) { return __builtin_fpclassify(FP_NAN, FP_INFINITE, FP_NORMAL, FP_SUBNORMAL, FP_ZERO, __x); }
inline bool isfinite(float __x) { return __builtin_isfinite(__x); }
inline bool isfinite(double __x) { return __builtin_isfinite(__x); }
inline bool isinf(float __x) { return __builtin_isinf(__x); }
inline bool isinf(double __x) { return __builtin_isinf(__x); }
inline bool isnan(float __x) { return __builtin_isnan(__x); }
inline bool isnan(double __x) { return __builtin_isnan(__x); }
inline bool isnormal(float __x) { return __builtin_isnormal(__x); }
inline bool isnormal(double __x) { return __builtin_isnormal(__x); }
inline bool signbit(float __x) { return __builtin_signbit(__x); }
inline bool signbit(double __x) { return __builtin_signbit(__x); }
inline bool isgreater(float __x, float __y) { return __builtin_isgreater(__x, __y); }
inline bool isgreater(double __x, double __y) { return __builtin_isgreater(__x, __y); }
inline bool isgreaterequal(float __x, float __y) { return __builtin_isgreaterequal(__x, __y); }
inline bool isgreaterequal(double __x, double __y) { return __builtin_isgreaterequal(__x, __y); }
inline bool isless(float __x, float __y) { return __builtin_isless(__x, __y); }
inline bool isless(double __x, double __y) { return __builtin_isless(__x, __y); }
inline bool islessequal(float __x, float __y) { return __builtin_islessequal(__x, __y); }
inline bool islessequal(double __x, double __y) { return __builtin_islessequal(__x, __y); }
inline bool islessgreater(float __x, float __y) { return __builtin_islessgreater(__x, __y); }
inline bool islessgreater(double __x, double __y) { return __builtin_islessgreater(__x, __y); }
inline bool isunordered(float __x, float __y) { return __builtin_isunordered(__x, __y); }
inline bool isunordered(double __x, double __y) { return __builtin_isunordered(__x, __y); }

extern "C" {
#endif

float fabsf(float __x);
double fabs(double __x);
float sqrtf(float __x);
double sqrt(double __x);
float floorf(float __x);
double floor(double __x);
float ceilf(float __x);
double ceil(double __x);
float fmodf(float __x, float __y);
double fmod(double __x, double __y);
float roundf(float __x);
double round(double __x);
float copysignf(float __x, float __y);
double copysign(double __x, double __y);
float nanf(const char *__tagp);
double nan(const char *__tagp);
float hypotf(float __x, float __y);
double hypot(double __x, double __y);
float logf(float __x);
double log(double __x);
float log2f(float __x);
double log2(double __x);
float powf(float __x, float __y);
double pow(double __x, double __y);
float sinf(float __x);
double sin(double __x);
float cosf(float __x);
double cos(double __x);
float expf(float __x);
double exp(double __x);
float frexpf(float __x, int *__exp);
double frexp(double __x, int *__exp);
float ldexpf(float __x, int __exp);
double ldexp(double __x, int __exp);
float modff(float __x, float *__iptr);
double modf(double __x, double *__iptr);
float log10f(float __x);
double log10(double __x);
float exp2f(float __x);
double exp2(double __x);
float expm1f(float __x);
double expm1(double __x);
float log1pf(float __x);
double log1p(double __x);
float cbrtf(float __x);
double cbrt(double __x);
float remainderf(float __x, float __y);
double remainder(double __x, double __y);
long lroundf(float __x);
long lround(double __x);
long long llroundf(float __x);
long long llround(double __x);
float truncf(float __x);
double trunc(double __x);
float nearbyintf(float __x);
double nearbyint(double __x);
float rintf(float __x);
double rint(double __x);
long lrintf(float __x);
long lrint(double __x);
long long llrintf(float __x);
long long llrint(double __x);
float atanf(float __x);
double atan(double __x);
float atan2f(float __y, float __x);
double atan2(double __y, double __x);
float tanf(float __x);
double tan(double __x);
float asinf(float __x);
double asin(double __x);
float acosf(float __x);
double acos(double __x);
float sinhf(float __x);
double sinh(double __x);
float coshf(float __x);
double cosh(double __x);
float tanhf(float __x);
double tanh(double __x);
float asinhf(float __x);
double asinh(double __x);
float acoshf(float __x);
double acosh(double __x);
float atanhf(float __x);
double atanh(double __x);
float erff(float __x);
double erf(double __x);
float erfcf(float __x);
double erfc(double __x);
float tgammaf(float __x);
double tgamma(double __x);
float lgammaf(float __x);
double lgamma(double __x);
float fmaf(float __x, float __y, float __z);
double fma(double __x, double __y, double __z);
float remquof(float __x, float __y, int *__quo);
double remquo(double __x, double __y, int *__quo);
float fmaxf(float __x, float __y);
double fmax(double __x, double __y);
float fminf(float __x, float __y);
double fmin(double __x, double __y);
float fdimf(float __x, float __y);
double fdim(double __x, double __y);
float nextafterf(float __x, float __y);
double nextafter(double __x, double __y);
float scalbnf(float __x, int __n);
double scalbn(double __x, int __n);
float scalblnf(float __x, long __n);
double scalbln(double __x, long __n);
int ilogbf(float __x);
int ilogb(double __x);
float logbf(float __x);
double logb(double __x);
long double nanl(const char *__tagp);

#ifdef __cplusplus
}
#endif

#endif // _TMS9900_MATH_H
