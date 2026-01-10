/*
 * Compact sinf/cosf for TMS9900
 * Uses polynomial approximation + minimal range reduction
 */

#include <stdint.h>

/* Range reduction constants */
static const float
pio2_hi = 1.5707963705e+00f,
pio2_lo = -4.3711388287e-08f,
invpio2 = 6.3661977237e-01f;

/* Polynomial coefficients for sin(x) on [-pi/4, pi/4] */
/* sin(x) ≈ x - x^3/6 + x^5/120 - x^7/5040 */
static const float
S1 = -1.6666666666e-01f,  /* -1/6 */
S2 =  8.3333333333e-03f,  /*  1/120 */
S3 = -1.9841269841e-04f;  /* -1/5040 */

/* Polynomial coefficients for cos(x) on [-pi/4, pi/4] */
/* cos(x) ≈ 1 - x^2/2 + x^4/24 - x^6/720 */
static const float
C1 = -5.0000000000e-01f,  /* -1/2 */
C2 =  4.1666666667e-02f,  /*  1/24 */
C3 = -1.3888888889e-03f;  /* -1/720 */

static inline float kernel_sinf(float x)
{
    float x2 = x * x;
    return x * (1.0f + x2 * (S1 + x2 * (S2 + x2 * S3)));
}

static inline float kernel_cosf(float x)
{
    float x2 = x * x;
    return 1.0f + x2 * (C1 + x2 * (C2 + x2 * C3));
}

static int rem_pio2f(float x, float *y)
{
    union { float f; uint32_t u; } ux = {x};
    uint32_t ix = ux.u & 0x7fffffff;
    int sign = ux.u >> 31;
    float z = sign ? -x : x;
    int n;
    float fn;

    if (ix <= 0x3f490fdb) { /* |x| <= pi/4 */
        *y = x;
        return 0;
    }

    fn = z * invpio2 + 0.5f;
    n = (int)fn;
    fn = (float)n;
    z = z - fn * pio2_hi - fn * pio2_lo;

    *y = sign ? -z : z;
    return sign ? -n : n;
}

float sinf(float x)
{
    float y;
    int n = rem_pio2f(x, &y);

    switch (n & 3) {
    case 0:  return  kernel_sinf(y);
    case 1:  return  kernel_cosf(y);
    case 2:  return -kernel_sinf(y);
    default: return -kernel_cosf(y);
    }
}

float cosf(float x)
{
    float y;
    int n = rem_pio2f(x, &y);

    switch (n & 3) {
    case 0:  return  kernel_cosf(y);
    case 1:  return -kernel_sinf(y);
    case 2:  return -kernel_cosf(y);
    default: return  kernel_sinf(y);
    }
}
