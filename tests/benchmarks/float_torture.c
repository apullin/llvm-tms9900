/* Float operations torture test
 *
 * Exercises IEEE-754 binary32 soft-float at runtime:
 * negation, comparison (including NaN and signed zero), int<->float
 * conversion, addition, subtraction, multiplication, and division.
 *
 * Opaque barriers prevent host compile-time constant folding so every
 * operation genuinely executes on the TMS9900 across -O0..-Oz and LTO.
 *
 * Expected: 29 tests pass.
 */
#include <stdint.h>

static uint32_t opaque_u32(uint32_t x) {
    uint16_t hi = (uint16_t)(x >> 16);
    uint16_t lo = (uint16_t)x;
    __asm__ volatile("" : "+r"(hi), "+r"(lo));
    return ((uint32_t)hi << 16) | (uint32_t)lo;
}

static int32_t opaque_i32(int32_t x) {
    return (int32_t)opaque_u32((uint32_t)x);
}

static float from_bits(uint32_t u) {
    union { uint32_t u; float f; } v;
    v.u = opaque_u32(u);
    return v.f;
}

static uint32_t to_bits(float f) {
    union { float f; uint32_t u; } v;
    v.f = f;
    return v.u;
}

/* IEEE 754 single-precision constants */
#define F_POS_ZERO  0x00000000UL  /* +0.0  */
#define F_NEG_ZERO  0x80000000UL  /* -0.0  */
#define F_POS_ONE   0x3F800000UL  /* +1.0  */
#define F_NEG_ONE   0xBF800000UL  /* -1.0  */
#define F_POS_TWO   0x40000000UL  /* +2.0  */
#define F_NEG_TWO   0xC0000000UL  /* -2.0  */
#define F_POS_THREE 0x40400000UL  /* +3.0  */
#define F_POS_SIX   0x40C00000UL  /* +6.0  */
#define F_NEG_SEVEN 0xC0E00000UL  /* -7.0  */
#define F_POS_42    0x42280000UL  /* +42.0 */
#define F_NEG_42    0xC2280000UL  /* -42.0 */
#define F_POS_256   0x43800000UL  /* +256.0 */
#define F_QNAN      0x7FC00000UL  /* quiet NaN */

#define TOTAL_TESTS 29

__attribute__((noinline)) static void halt_ok(void) { __asm__ volatile("idle"); }
__attribute__((noinline)) static void fail_loop(void) { for (;;) {} }

volatile uint16_t result;

int main(void) {
    uint16_t pass = 0;

    float pos_zero  = from_bits(F_POS_ZERO);
    float neg_zero  = from_bits(F_NEG_ZERO);
    float pos_one   = from_bits(F_POS_ONE);
    float neg_one   = from_bits(F_NEG_ONE);
    float pos_two   = from_bits(F_POS_TWO);
    float pos_three = from_bits(F_POS_THREE);
    float pos_six   = from_bits(F_POS_SIX);
    float pos_42    = from_bits(F_POS_42);
    float neg_42    = from_bits(F_NEG_42);
    float qnan      = from_bits(F_QNAN);

    /* ---- Negation tests ---- */
    if (to_bits(-pos_one) == F_NEG_ONE) pass++;
    if (to_bits(-neg_one) == F_POS_ONE) pass++;
    if (to_bits(-pos_zero) == F_NEG_ZERO) pass++;

    /* ---- Comparison tests ---- */
    if (pos_one < pos_two) pass++;
    if (pos_two > pos_one) pass++;
    if (pos_one == from_bits(F_POS_ONE)) pass++;
    if (neg_one < pos_one) pass++;
    if (pos_zero == neg_zero) pass++;
    if (!(qnan == qnan)) pass++;
    if (!(qnan < pos_one)) pass++;
    if (!(qnan > pos_one)) pass++;
    if (!(pos_one < pos_two && pos_two < pos_one)) pass++;

    /* ---- Int->float conversion tests ---- */
    if (to_bits((float)opaque_i32(0)) == F_POS_ZERO) pass++;
    if (to_bits((float)opaque_i32(1)) == F_POS_ONE) pass++;
    if (to_bits((float)opaque_i32(-1)) == F_NEG_ONE) pass++;
    if (to_bits((float)opaque_i32(42)) == F_POS_42) pass++;
    if (to_bits((float)opaque_u32(0)) == F_POS_ZERO) pass++;
    if (to_bits((float)opaque_u32(1)) == F_POS_ONE) pass++;
    if (to_bits((float)opaque_u32(256)) == F_POS_256) pass++;

    /* ---- Float->int conversion tests ---- */
    if ((int32_t)pos_one == 1) pass++;
    if ((int32_t)neg_one == -1) pass++;
    if ((uint32_t)pos_one == 1) pass++;
    if ((uint32_t)pos_42 == 42) pass++;

    /* ---- Arithmetic tests (add, sub, mul, div) ---- */
    if (to_bits(pos_one + pos_two) == F_POS_THREE) pass++;
    if (to_bits(pos_three - pos_one) == F_POS_TWO) pass++;
    if (to_bits(pos_one - pos_three) == F_NEG_TWO) pass++;
    if (to_bits(pos_two * pos_three) == F_POS_SIX) pass++;
    if (to_bits(pos_six / pos_two) == F_POS_THREE) pass++;
    if (to_bits(neg_42 / pos_six) == F_NEG_SEVEN) pass++;

    result = pass;

    if (pass == TOTAL_TESTS)
        halt_ok();

    fail_loop();
    return 0;
}
