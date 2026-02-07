/* 3D fixed-point vertex transformation benchmark
 *
 * Transforms 8 cube vertices through a combined Ry(30)*Rx(45) rotation
 * matrix using Q8.8 fixed-point arithmetic, projects to 2D (orthographic),
 * and verifies a hash checksum of the results.
 *
 * Exercises:
 *   - Signed 16x16->32 multiply (Q8.8 fixed-point)
 *   - 32-bit intermediate results with shift-back-to-16
 *   - Struct/array access patterns (vertex arrays, 3x3 matrix)
 *   - Loop-heavy code (matrix-vector multiply over 8 vertices)
 *   - Mixed 16/32-bit arithmetic for checksum
 *
 * Q8.8 format: 1 sign bit, 7 integer bits, 8 fractional bits
 * Range: -128.0 to +127.996, resolution: 1/256 ~ 0.0039
 *
 * Expected checksum: 0xAB7C5300
 */
#include <stdint.h>

__attribute__((noinline)) static void halt_ok(void) { __asm__ volatile("idle"); }
__attribute__((noinline)) static void fail_loop(void) { for (;;) {} }

/* Q8.8 fixed-point type */
typedef int16_t fixed_t;

#define FP_SHIFT 8
#define FP_ONE   (1 << FP_SHIFT)   /* 256 = 1.0 */
#define NUM_VERTS 8

/* Fixed-point multiply: (a * b) >> 8 */
__attribute__((noinline))
static fixed_t fp_mul(fixed_t a, fixed_t b) {
    int32_t result = (int32_t)a * (int32_t)b;
    return (fixed_t)(result >> FP_SHIFT);
}

typedef struct { fixed_t x, y, z; } vec3_t;
typedef struct { fixed_t x, y; } vec2_t;

/* Pre-computed combined rotation matrix: Rx(45) * Ry(30)
 *
 * Ry(30):  cos30=222  sin30=128  (Q8.8)
 *   | 222    0   128 |
 *   |   0  256     0 |
 *   |-128    0   222 |
 *
 * Rx(45):  cos45=181  sin45=181  (Q8.8)
 *   | 256    0     0 |
 *   |   0  181  -181 |
 *   |   0  181   181 |
 *
 * Combined = Rx * Ry (computed in Q8.8):
 *   | 222    0   128 |
 *   |  90  181  -157 |
 *   | -91  181   156 |
 */
static const fixed_t rot[3][3] = {
    {  222,    0,  128 },
    {   90,  181, -157 },
    {  -91,  181,  156 },
};

/* Cube vertices: (+-1.0, +-1.0, +-1.0) in Q8.8 */
static const vec3_t cube[NUM_VERTS] = {
    {  FP_ONE,  FP_ONE,  FP_ONE },
    {  FP_ONE,  FP_ONE, -FP_ONE },
    {  FP_ONE, -FP_ONE,  FP_ONE },
    {  FP_ONE, -FP_ONE, -FP_ONE },
    { -FP_ONE,  FP_ONE,  FP_ONE },
    { -FP_ONE,  FP_ONE, -FP_ONE },
    { -FP_ONE, -FP_ONE,  FP_ONE },
    { -FP_ONE, -FP_ONE, -FP_ONE },
};

/* Output: 2D projected vertices (volatile to prevent optimization) */
volatile vec2_t proj[NUM_VERTS];
volatile uint32_t result;

/* Expected Bernstein hash of all projected 2D coordinates */
#define EXPECTED_CKSUM 0xAB7C5300u

int main(void) {
    uint32_t cksum;
    int16_t i;

    /* Transform each vertex: multiply by rotation matrix, project to 2D */
    for (i = 0; i < NUM_VERTS; i++) {
        fixed_t vx = cube[i].x;
        fixed_t vy = cube[i].y;
        fixed_t vz = cube[i].z;

        /* Matrix-vector multiply: rotated = rot * vertex */
        fixed_t rx = fp_mul(rot[0][0], vx) + fp_mul(rot[0][1], vy) + fp_mul(rot[0][2], vz);
        fixed_t ry = fp_mul(rot[1][0], vx) + fp_mul(rot[1][1], vy) + fp_mul(rot[1][2], vz);
        /* rz not needed for orthographic projection (drop Z) */

        /* Orthographic projection: just take (rx, ry) */
        proj[i].x = rx;
        proj[i].y = ry;
    }

    /* Compute Bernstein hash (variant with multiplier 31) over projected coords.
     * cksum = cksum * 31 + value  ==  (cksum << 5) - cksum + value
     * This uses 32-bit shifts and subtraction, exercising the TMS9900's
     * multi-word arithmetic.
     */
    cksum = 0;
    for (i = 0; i < NUM_VERTS; i++) {
        uint16_t ux = (uint16_t)proj[i].x;
        uint16_t uy = (uint16_t)proj[i].y;

        cksum = (cksum << 5) - cksum + (uint32_t)ux;
        cksum = (cksum << 5) - cksum + (uint32_t)uy;
    }

    result = cksum;

    if (cksum == EXPECTED_CKSUM)
        halt_ok();

    fail_loop();
    return 0;
}
