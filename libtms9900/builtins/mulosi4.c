/* libtms9900 - Checked 32-bit multiply with overflow flag
 *
 * __mulosi4: 32-bit multiply, sets *overflow = 1 on overflow
 *
 * si_int = int32_t = long on TMS9900.
 */

typedef long si_int;
typedef unsigned long su_int;

extern long __mulsi3(long a, long b);

#define SI_MIN ((si_int)((su_int)1 << 31))
#define SI_MAX (~SI_MIN)

si_int __mulosi4(si_int a, si_int b, int *overflow) {
    *overflow = 0;
    si_int result = __mulsi3(a, b);
    if (a == SI_MIN) {
        if (b != 0 && b != 1)
            *overflow = 1;
        return result;
    }
    if (b == SI_MIN) {
        if (a != 0 && a != 1)
            *overflow = 1;
        return result;
    }
    si_int sa = a >> 31;
    si_int abs_a = (a ^ sa) - sa;
    si_int sb = b >> 31;
    si_int abs_b = (b ^ sb) - sb;
    if (abs_a < 2 || abs_b < 2)
        return result;
    if (sa == sb) {
        if (abs_a > SI_MAX / abs_b)
            *overflow = 1;
    } else {
        if (abs_a > SI_MIN / -abs_b)
            *overflow = 1;
    }
    return result;
}
