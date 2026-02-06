/* Bubble sort benchmark (ported from i8085 project)
 *
 * Sorts 16 int16_t values using bubble sort.
 * Exercises 16-bit signed comparisons, swaps, nested loops.
 */
#include <stdint.h>

__attribute__((noinline)) static void halt_ok(void) { __asm__ volatile("idle"); }
__attribute__((noinline)) static void fail_loop(void) { for (;;) {} }

static const int16_t input[16] = {
    100, -500, 32767, 0, -32768, 1024, -1, 42,
    -100, 500, 256, -256, 10000, -10000, 7, 3
};

volatile int16_t output[16];

int main(void) {
    int16_t arr[16];
    for (uint16_t i = 0; i < 16; i++)
        arr[i] = input[i];

    for (uint16_t i = 0; i < 15; i++) {
        for (uint16_t j = 0; j < 15 - i; j++) {
            if (arr[j] > arr[j + 1]) {
                int16_t tmp = arr[j];
                arr[j] = arr[j + 1];
                arr[j + 1] = tmp;
            }
        }
    }

    for (uint16_t i = 0; i < 16; i++)
        output[i] = arr[i];

    if (arr[0] == -32768 && arr[15] == 32767)
        halt_ok();

    fail_loop();
    return 0;
}
