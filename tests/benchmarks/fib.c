/* Fibonacci benchmark (ported from i8085 project)
 *
 * Computes first 16 Fibonacci numbers.
 * Exercises 16-bit addition in a simple loop.
 * result[15] = 610 = 0x0262
 */
#include <stdint.h>

__attribute__((noinline)) static void halt_ok(void) { __asm__ volatile("idle"); }
__attribute__((noinline)) static void fail_loop(void) { for (;;) {} }

volatile uint16_t result[16];

int main(void) {
    result[0] = 0;
    result[1] = 1;
    for (uint16_t i = 2; i < 16; i++) {
        result[i] = result[i - 1] + result[i - 2];
    }

    /* Fib(15) = 610 */
    if (result[15] == 610)
        halt_ok();

    fail_loop();
    return 0;
}
