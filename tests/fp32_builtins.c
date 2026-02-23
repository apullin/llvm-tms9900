#include <stdint.h>

/* 32-bit builtins (__mulsi3, __ashlsi3, __lshrsi3, __ashrsi3, __udivsi3,
 * __divsi3, __umodsi3, __modsi3) are provided by libtms9900/builtins/ as
 * hand-coded assembly. Do NOT duplicate them here — the C versions have a
 * calling-convention mismatch: the compiler passes shift counts in R2
 * (16-bit), but C implementations compiled with int32_t parameters read R3
 * (low word of a 32-bit R2:R3 pair). */

/* 64-bit builtins (__muldi3, __ashldi3, __ashrdi3, __lshrdi3) are now
 * provided by libbuiltins.a (compiler-rt reference implementations with
 * correct 'int' parameter ABI). Do NOT duplicate them here. */

int __fe_getround(void) {
    return 0;
}

int __fe_raise_inexact(void) {
    return 0;
}

static float make_inf(uint32_t sign) {
    union {
        uint32_t u;
        float f;
    } conv;
    conv.u = sign ? 0xFF800000u : 0x7F800000u;
    return conv.f;
}

static float make_nan(void) {
    union {
        uint32_t u;
        float f;
    } conv;
    conv.u = 0x7FC00000u;
    return conv.f;
}

float __math_divzerof(uint32_t sign) {
    return make_inf(sign);
}

float __math_invalidf(float x) {
    (void)x;
    return make_nan();
}

float __math_oflowf(uint32_t sign) {
    return make_inf(sign);
}

float __math_uflowf(uint32_t sign) {
    (void)sign;
    return 0.0f;
}
