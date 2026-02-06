/* Float operations torture test (ported from i8085 project)
 *
 * Exercises soft-float: negation, comparison, int<->float conversion,
 * addition, multiplication.
 *
 * Uses native float type — compiler emits soft-float calls automatically.
 *
 * Expected: 24 tests pass.
 */
#include <stdint.h>

/* Helper to create float from IEEE 754 bit pattern */
static float from_bits(uint32_t u) {
    union { uint32_t u; float f; } v;
    v.u = u;
    return v.f;
}

static uint32_t to_bits(float f) {
    union { float f; uint32_t u; } v;
    v.f = f;
    return v.u;
}

/* IEEE 754 single-precision constants */
#define F_POS_ZERO  0x00000000U  /* +0.0  */
#define F_NEG_ZERO  0x80000000U  /* -0.0  */
#define F_POS_ONE   0x3F800000U  /* +1.0  */
#define F_NEG_ONE   0xBF800000U  /* -1.0  */
#define F_POS_TWO   0x40000000U  /* +2.0  */
#define F_POS_THREE 0x40400000U  /* +3.0  */
#define F_POS_SIX   0x40C00000U  /* +6.0  */
#define F_POS_42    0x42280000U  /* +42.0 */
#define F_POS_256   0x43800000U  /* +256.0 */
#define F_QNAN      0x7FC00000U  /* quiet NaN */

#define TOTAL_TESTS 24

__attribute__((noinline)) static void halt_ok(void) { __asm__ volatile("idle"); }
__attribute__((noinline)) static void fail_loop(void) { for (;;) {} }

volatile uint16_t result;

int main(void) {
    volatile uint16_t pass = 0;

    float pos_zero  = from_bits(F_POS_ZERO);
    float neg_zero  = from_bits(F_NEG_ZERO);
    float pos_one   = from_bits(F_POS_ONE);
    float neg_one   = from_bits(F_NEG_ONE);
    float pos_two   = from_bits(F_POS_TWO);
    float pos_three = from_bits(F_POS_THREE);
    float pos_six   = from_bits(F_POS_SIX);
    float pos_42    = from_bits(F_POS_42);
    float pos_256   = from_bits(F_POS_256);
    float qnan      = from_bits(F_QNAN);

    /* ---- Negation tests ---- */

    /* 1. neg(+1.0) == -1.0 */
    if (to_bits(-pos_one) == F_NEG_ONE) pass++;

    /* 2. neg(-1.0) == +1.0 */
    if (to_bits(-neg_one) == F_POS_ONE) pass++;

    /* 3. neg(+0.0) == -0.0 */
    if (to_bits(-pos_zero) == F_NEG_ZERO) pass++;

    /* ---- Comparison tests ---- */

    /* 4. 1.0 < 2.0 */
    if (pos_one < pos_two) pass++;

    /* 5. 2.0 > 1.0 */
    if (pos_two > pos_one) pass++;

    /* 6. 1.0 == 1.0 */
    if (pos_one == pos_one) pass++;

    /* 7. -1.0 < 1.0 */
    if (neg_one < pos_one) pass++;

    /* 8. NaN != NaN (unordered) */
    if (!(qnan == qnan)) pass++;

    /* 9. NaN is not less than 1.0 */
    if (!(qnan < pos_one)) pass++;

    /* 10. NaN is not greater than 1.0 */
    if (!(qnan > pos_one)) pass++;

    /* 11. 1.0 is ordered with 2.0 */
    if (pos_one == pos_one) pass++;

    /* ---- Int->float conversion tests ---- */

    /* 12. (float)0 == 0.0 */
    if (to_bits((float)(int32_t)0) == F_POS_ZERO) pass++;

    /* 13. (float)1 == 1.0 */
    if (to_bits((float)(int32_t)1) == F_POS_ONE) pass++;

    /* 14. (float)(-1) == -1.0 */
    if (to_bits((float)(int32_t)-1) == F_NEG_ONE) pass++;

    /* 15. (float)42 == 42.0 */
    if (to_bits((float)(int32_t)42) == F_POS_42) pass++;

    /* 16. (float)(unsigned)0 == 0.0 */
    if (to_bits((float)(uint32_t)0) == F_POS_ZERO) pass++;

    /* 17. (float)(unsigned)1 == 1.0 */
    if (to_bits((float)(uint32_t)1) == F_POS_ONE) pass++;

    /* 18. (float)(unsigned)256 == 256.0 */
    if (to_bits((float)(uint32_t)256) == F_POS_256) pass++;

    /* ---- Float->int conversion tests ---- */

    /* 19. (int)1.0 == 1 */
    if ((int32_t)pos_one == 1) pass++;

    /* 20. (int)(-1.0) == -1 */
    if ((int32_t)neg_one == -1) pass++;

    /* 21. (unsigned)1.0 == 1 */
    if ((uint32_t)pos_one == 1) pass++;

    /* 22. (unsigned)42.0 == 42 */
    if ((uint32_t)pos_42 == 42) pass++;

    /* ---- Arithmetic tests ---- */

    /* 23. 1.0 + 2.0 == 3.0 */
    if (to_bits(pos_one + pos_two) == F_POS_THREE) pass++;

    /* 24. 2.0 * 3.0 == 6.0 */
    if (to_bits(pos_two * pos_three) == F_POS_SIX) pass++;

    result = pass;

    if (pass == TOTAL_TESTS)
        halt_ok();

    fail_loop();
    return 0;
}
