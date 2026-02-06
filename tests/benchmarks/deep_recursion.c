/* Deep recursion benchmark (ported from i8085 project)
 *
 * Tests deep recursion with sum_down(64).
 * Exercises stack management and function call overhead.
 * Result: 64*65/2 = 2080 = 0x820, truncated to uint8_t = 0x20.
 */
#include <stdint.h>

#ifndef DEPTH
#define DEPTH 64u
#endif

__attribute__((noinline)) static uint8_t sum_down(uint8_t n) {
    if (n == 0) return 0;
    return (uint8_t)(n + sum_down((uint8_t)(n - 1u)));
}

__attribute__((noinline)) static void halt_ok(void) { __asm__ volatile("idle"); }
__attribute__((noinline)) static void fail_loop(void) { for (;;) {} }

volatile uint8_t result;

int main(void) {
    uint8_t res = sum_down((uint8_t)DEPTH);
    result = res;

    if (res == 0x20u)
        halt_ok();

    fail_loop();
    return 0;
}
