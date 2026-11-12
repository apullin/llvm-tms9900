/* Bubble sort benchmark
 *
 * Sorts 16 int16_t values using bubble sort from an opaque input pointer
 * and verifies the complete sorted sequence.
 * Exercises 16-bit signed comparisons, swaps, and nested loops.
 */
#include <stdint.h>

__attribute__((noinline)) static void halt_ok(void) { __asm__ volatile("idle"); }
__attribute__((noinline)) static void fail_loop(void) { for (;;) {} }

static const int16_t input[16] = {
    100, -500, 32767, 0, -32768, 1024, -1, 42,
    -100, 500, 256, -256, 10000, -10000, 7, 3
};

static const int16_t expected[16] = {
    -32768, -10000, -500, -256, -100, -1, 0, 3,
    7, 42, 100, 256, 500, 1024, 10000, 32767
};

volatile int16_t output[16];

int main(void) {
    const int16_t *src = input;
    __asm__ volatile("" : "+r"(src));

    int16_t arr[16];
    for (uint16_t i = 0; i < 16; i++)
        arr[i] = src[i];

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

    for (uint16_t i = 0; i < 16; i++) {
        if (arr[i] != expected[i])
            fail_loop();
    }

    halt_ok();
    return 0;
}
