/* Q7.8 fixed-point 2x2 matrix multiply
 *
 * Multiplies two 2x2 matrices in Q7.8 fixed-point format at runtime
 * (opaque input pointers prevent compile-time constant folding).
 * Exercises signed 16x16->32 multiply, shifts, and 16-bit addition.
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

__attribute__((noinline))
static void matmul_2x2_q7_8(volatile int16_t *out,
                            const int16_t *a, const int16_t *b) {
    out[0] = q7_8_mul(a[0], b[0]) + q7_8_mul(a[1], b[2]);
    out[1] = q7_8_mul(a[0], b[1]) + q7_8_mul(a[1], b[3]);
    out[2] = q7_8_mul(a[2], b[0]) + q7_8_mul(a[3], b[2]);
    out[3] = q7_8_mul(a[2], b[1]) + q7_8_mul(a[3], b[3]);
}

static const int16_t A[4] = { 0x0180, 0x00C0, 0x0040, 0x0200 };
static const int16_t B[4] = { 0x0100, 0x0080, 0x0080, 0x0100 };

volatile int16_t C[4];

int main(void) {
    const int16_t *pa = A;
    const int16_t *pb = B;
    __asm__ volatile("" : "+r"(pa), "+r"(pb));

    matmul_2x2_q7_8(C, pa, pb);

    if (C[0] == 0x01E0 && C[1] == 0x0180 &&
        C[2] == 0x0140 && C[3] == 0x0220)
        halt_ok();

    fail_loop();
    return 0;
}
