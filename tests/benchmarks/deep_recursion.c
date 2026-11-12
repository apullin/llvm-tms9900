/* Deep recursion & tail-call benchmark
 *
 * Exercises both:
 *  1. Genuine 64-deep non-tail call/stack frames (sum_frames), where a
 *     post-call inline-asm value barrier keeps each stack frame live across BL.
 *  2. Genuine tail-call recursion (tail_mix), which the optimizer converts
 *     into a zero-frame loop without closed-form algebraic collapse.
 */
#include <stdint.h>

#ifndef DEPTH
#define DEPTH 64u
#endif

__attribute__((noinline))
static uint16_t sum_frames(uint16_t n) {
    if (n == 0)
        return 0;
    uint16_t sub = sum_frames((uint16_t)(n - 1u));
    __asm__ volatile("" : "+r"(sub));
    return (uint16_t)(n + sub);
}

__attribute__((noinline))
static uint16_t tail_mix(uint16_t n, uint16_t acc) {
    if (n == 0)
        return acc;
    uint16_t next = (uint16_t)((acc ^ (uint16_t)(n * 257u)) + n);
    return tail_mix((uint16_t)(n - 1u), next);
}

__attribute__((noinline)) static void halt_ok(void) { __asm__ volatile("idle"); }
__attribute__((noinline)) static void fail_loop(void) { for (;;) {} }

volatile uint16_t result_frames;
volatile uint16_t result_tail;

int main(void) {
    uint16_t d = DEPTH;
    __asm__ volatile("" : "+r"(d));

    uint16_t r1 = sum_frames(d);
    uint16_t r2 = tail_mix(d, 0);
    result_frames = r1;
    result_tail = r2;

    if (r1 == 0x0820u && r2 == 0x4D00u)
        halt_ok();

    fail_loop();
    return 0;
}
