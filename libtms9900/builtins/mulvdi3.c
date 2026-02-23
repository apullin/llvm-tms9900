/* libtms9900 - Checked 64-bit multiply operations
 *
 * __mulvdi3: 64-bit multiply with overflow trap
 * __mulodi4: 64-bit multiply with overflow flag
 *
 * These are complex operations that use 64-bit division for overflow
 * detection. Kept as C to call existing __muldi3/__divdi3.
 *
 * si_int = int32_t on TMS9900 (compiler-rt convention).
 * di_int = int64_t.
 */

typedef long long di_int;
typedef unsigned long long du_int;
typedef long si_int;
typedef unsigned long su_int;

extern void abort(void);

/* Forward declarations for builtins we call */
extern di_int __muldi3(di_int a, di_int b);
extern di_int __divdi3(di_int a, di_int b);

#define DI_MIN ((di_int)((du_int)1 << 63))
#define DI_MAX (~DI_MIN)

/* __mulvdi3: a * b, abort on overflow */
di_int __mulvdi3(di_int a, di_int b) {
    if (a == DI_MIN) {
        if (b == 0 || b == 1)
            return a * b;
        abort();
    }
    if (b == DI_MIN) {
        if (a == 0 || a == 1)
            return a * b;
        abort();
    }
    di_int sa = a >> 63;
    di_int abs_a = (a ^ sa) - sa;
    di_int sb = b >> 63;
    di_int abs_b = (b ^ sb) - sb;
    if (abs_a < 2 || abs_b < 2)
        return a * b;
    if (sa == sb) {
        if (abs_a > DI_MAX / abs_b)
            abort();
    } else {
        if (abs_a > DI_MIN / -abs_b)
            abort();
    }
    return a * b;
}

/* __mulodi4: a * b, sets *overflow = 1 on overflow */
di_int __mulodi4(di_int a, di_int b, int *overflow) {
    *overflow = 0;
    di_int result = a * b;
    if (a == DI_MIN) {
        if (b != 0 && b != 1)
            *overflow = 1;
        return result;
    }
    if (b == DI_MIN) {
        if (a != 0 && a != 1)
            *overflow = 1;
        return result;
    }
    di_int sa = a >> 63;
    di_int abs_a = (a ^ sa) - sa;
    di_int sb = b >> 63;
    di_int abs_b = (b ^ sb) - sb;
    if (abs_a < 2 || abs_b < 2)
        return result;
    if (sa == sb) {
        if (abs_a > DI_MAX / abs_b)
            *overflow = 1;
    } else {
        if (abs_a > DI_MIN / -abs_b)
            *overflow = 1;
    }
    return result;
}
