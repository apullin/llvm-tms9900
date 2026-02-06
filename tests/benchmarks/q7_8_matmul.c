/* Q7.8 fixed-point 2x2 matrix multiply (ported from i8085 project)
 *
 * Multiplies two 2x2 matrices in Q7.8 fixed-point format.
 * Exercises 16-bit multiply (via 32-bit intermediate), shifts,
 * and 16-bit addition.
 *
 * A = | 1.5   0.75 |   = | 0x0180  0x00C0 |
 *     | 0.25  2.0  |     | 0x0040  0x0200 |
 *
 * B = | 1.0   0.5  |   = | 0x0100  0x0080 |
 *     | 0.5   1.0  |     | 0x0080  0x0100 |
 *
 * C = A * B = | 1.875   1.5   |  = | 0x01E0  0x0180 |
 *             | 1.25    2.125 |     | 0x0140  0x0220 |
 */
#include <stdint.h>

__attribute__((noinline)) static void halt_ok(void) { __asm__ volatile("idle"); }
__attribute__((noinline)) static void fail_loop(void) { for (;;) {} }

static int16_t q7_8_mul(int16_t a, int16_t b) {
    return (int16_t)(((int32_t)a * (int32_t)b) >> 8);
}

static const int16_t A[4] = { 0x0180, 0x00C0, 0x0040, 0x0200 };
static const int16_t B[4] = { 0x0100, 0x0080, 0x0080, 0x0100 };

volatile int16_t C[4];

int main(void) {
    /* 2x2 matrix multiply: C = A * B */
    C[0] = q7_8_mul(A[0], B[0]) + q7_8_mul(A[1], B[2]);
    C[1] = q7_8_mul(A[0], B[1]) + q7_8_mul(A[1], B[3]);
    C[2] = q7_8_mul(A[2], B[0]) + q7_8_mul(A[3], B[2]);
    C[3] = q7_8_mul(A[2], B[1]) + q7_8_mul(A[3], B[3]);

    /* Verify: C = | 0x01E0  0x0180 | */
    /*             | 0x0140  0x0220 | */
    if (C[0] == 0x01E0 && C[1] == 0x0180 &&
        C[2] == 0x0140 && C[3] == 0x0220)
        halt_ok();

    fail_loop();
    return 0;
}
