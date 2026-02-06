#include <stdint.h>

/* 32-bit builtins (__mulsi3, __ashlsi3, __lshrsi3, __ashrsi3, __udivsi3,
 * __divsi3, __umodsi3, __modsi3) are provided by libtms9900/builtins/ as
 * hand-coded assembly. Do NOT duplicate them here — the C versions have a
 * calling-convention mismatch: the compiler passes shift counts in R2
 * (16-bit), but C implementations compiled with int32_t parameters read R3
 * (low word of a 32-bit R2:R3 pair). */

typedef union {
    uint32_t u32;
    struct {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        uint16_t hi;
        uint16_t lo;
#else
        uint16_t lo;
        uint16_t hi;
#endif
    } w;
} u32_parts;

typedef union {
    uint64_t u64;
    struct {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        uint32_t hi;
        uint32_t lo;
#else
        uint32_t lo;
        uint32_t hi;
#endif
    } w;
} u64_parts;

static void mul_u32_u32(uint32_t a, uint32_t b, uint32_t *hi, uint32_t *lo) {
    uint32_t a_lo = a & 0xFFFFu;
    uint32_t a_hi = a >> 16;
    uint32_t b_lo = b & 0xFFFFu;
    uint32_t b_hi = b >> 16;

    uint32_t p0 = a_lo * b_lo;
    uint32_t p1 = a_lo * b_hi;
    uint32_t p2 = a_hi * b_lo;
    uint32_t p3 = a_hi * b_hi;

    uint32_t mid = (p1 & 0xFFFFu) + (p2 & 0xFFFFu) + (p0 >> 16);
    *lo = (p0 & 0xFFFFu) | (mid << 16);
    *hi = p3 + (p1 >> 16) + (p2 >> 16) + (mid >> 16);
}

uint64_t __muldi3(uint64_t a, uint64_t b) {
    u64_parts ua;
    u64_parts ub;
    u64_parts res;
    uint32_t p0_hi, p0_lo;
    uint32_t p1_hi, p1_lo;
    uint32_t p2_hi, p2_lo;

    ua.u64 = a;
    ub.u64 = b;

    mul_u32_u32(ua.w.lo, ub.w.lo, &p0_hi, &p0_lo);
    mul_u32_u32(ua.w.lo, ub.w.hi, &p1_hi, &p1_lo);
    mul_u32_u32(ua.w.hi, ub.w.lo, &p2_hi, &p2_lo);
    (void)p1_hi;
    (void)p2_hi;

    res.w.lo = p0_lo;
    res.w.hi = p0_hi + p1_lo + p2_lo;
    return res.u64;
}

uint64_t __ashldi3(uint64_t a, int32_t b) {
    u64_parts v;
    uint32_t shift = (uint32_t)b;

    v.u64 = a;
    if (shift == 0) {
        return v.u64;
    }
    if (shift >= 64) {
        v.w.hi = 0;
        v.w.lo = 0;
        return v.u64;
    }
    if (shift >= 32) {
        v.w.hi = v.w.lo << (shift - 32);
        v.w.lo = 0;
        return v.u64;
    }

    v.w.hi = (v.w.hi << shift) | (v.w.lo >> (32 - shift));
    v.w.lo <<= shift;
    return v.u64;
}

uint64_t __lshrdi3(uint64_t a, int32_t b) {
    u64_parts v;
    uint32_t shift = (uint32_t)b;

    v.u64 = a;
    if (shift == 0) {
        return v.u64;
    }
    if (shift >= 64) {
        v.w.hi = 0;
        v.w.lo = 0;
        return v.u64;
    }
    if (shift >= 32) {
        v.w.lo = v.w.hi >> (shift - 32);
        v.w.hi = 0;
        return v.u64;
    }

    v.w.lo = (v.w.lo >> shift) | (v.w.hi << (32 - shift));
    v.w.hi >>= shift;
    return v.u64;
}

int64_t __ashrdi3(int64_t a, int32_t b) {
    u64_parts v;
    uint32_t shift = (uint32_t)b;
    int32_t hi;

    v.u64 = (uint64_t)a;
    if (shift == 0) {
        return (int64_t)v.u64;
    }
    if (shift >= 64) {
        hi = (int32_t)v.w.hi >> 31;
        v.w.hi = (uint32_t)hi;
        v.w.lo = (uint32_t)hi;
        return (int64_t)v.u64;
    }

    hi = (int32_t)v.w.hi;
    if (shift >= 32) {
        v.w.lo = (uint32_t)(hi >> (shift - 32));
        v.w.hi = (uint32_t)(hi >> 31);
        return (int64_t)v.u64;
    }

    v.w.lo = (v.w.lo >> shift) | ((uint32_t)hi << (32 - shift));
    v.w.hi = (uint32_t)(hi >> shift);
    return (int64_t)v.u64;
}

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
