/* 64-bit arithmetic torture test for TMS9900
 *
 * Exercises the i64 runtime library functions:
 *   - __muldi3:    64-bit multiply
 *   - __udivdi3:   unsigned 64-bit divide
 *   - __umoddi3:   unsigned 64-bit modulo
 *   - __divdi3:    signed 64-bit divide
 *   - __moddi3:    signed 64-bit modulo
 *
 * Also exercises inline i64 add/sub/shift (which the compiler expands).
 *
 * Expected checksum: 0x3F5A
 */

typedef unsigned int uint16_t;
typedef unsigned long uint32_t;
typedef unsigned long long uint64_t;
typedef long long int64_t;

__attribute__((noinline)) static void halt_ok(void) { __asm__ volatile("idle"); }
__attribute__((noinline)) static void fail_loop(void) { for (;;) {} }

/* Prevent constant folding */
__attribute__((noinline, optnone)) static uint64_t id64u(uint64_t x) { return x; }
__attribute__((noinline, optnone)) static int64_t id64s(int64_t x) { return x; }

/* Fold a 64-bit value into a 16-bit checksum by XORing all four 16-bit words */
__attribute__((noinline)) static uint16_t fold64(uint16_t check, uint64_t val) {
    check ^= (uint16_t)(val >> 48);
    check ^= (uint16_t)(val >> 32);
    check ^= (uint16_t)(val >> 16);
    check ^= (uint16_t)val;
    return check;
}

int main(void) {
    uint16_t check = 0;
    uint64_t u;
    int64_t s;

    /* === Multiply tests === */

    /* T1: basic multiply (small values) */
    u = id64u(100) * id64u(200);  /* 20000 */
    check = fold64(check, u);

    /* T2: multiply crossing 32-bit boundary */
    u = id64u(0x10000ULL) * id64u(0x10000ULL);  /* 0x100000000 */
    check = fold64(check, u);

    /* T3: larger multiply */
    u = id64u(0x12345678ULL) * id64u(0xABCDEF01ULL);
    check = fold64(check, u);

    /* T4: multiply with high bits set */
    u = id64u(0x100000001ULL) * id64u(0x100000001ULL);
    check = fold64(check, u);

    /* T5: multiply by 0 */
    u = id64u(0x123456789ABCDEF0ULL) * id64u(0);
    check = fold64(check, u);

    /* T6: multiply by 1 */
    u = id64u(0x123456789ABCDEF0ULL) * id64u(1);
    check = fold64(check, u);

    /* T7: multiply -1 * 42 (unsigned view) */
    u = id64u(0xFFFFFFFFFFFFFFFFULL) * id64u(42);
    check = fold64(check, u);

    /* === Unsigned divide tests === */

    /* T8: basic unsigned divide */
    u = id64u(1000) / id64u(7);  /* 142 */
    check = fold64(check, u);

    /* T9: large unsigned divide */
    u = id64u(0x123456789ABCDEF0ULL) / id64u(0x1234ULL);
    check = fold64(check, u);

    /* T10: dividend < divisor */
    u = id64u(5) / id64u(100);  /* 0 */
    check = fold64(check, u);

    /* T11: self-divide */
    u = id64u(0xABCDEF0123456789ULL) / id64u(0xABCDEF0123456789ULL);  /* 1 */
    check = fold64(check, u);

    /* === Unsigned modulo tests === */

    /* T12: basic unsigned modulo */
    u = id64u(1000) % id64u(7);  /* 6 */
    check = fold64(check, u);

    /* T13: large unsigned modulo */
    u = id64u(0x123456789ABCDEF0ULL) % id64u(0x1234ULL);
    check = fold64(check, u);

    /* === Signed divide tests === */

    /* T14: signed divide positive / positive */
    s = id64s(1000) / id64s(7);  /* 142 */
    check = fold64(check, (uint64_t)s);

    /* T15: signed divide negative / positive */
    s = id64s(-1000LL) / id64s(7);  /* -142 */
    check = fold64(check, (uint64_t)s);

    /* T16: signed divide negative / negative */
    s = id64s(-1000LL) / id64s(-7LL);  /* 142 */
    check = fold64(check, (uint64_t)s);

    /* === Signed modulo test === */

    /* T17: signed modulo negative % positive */
    s = id64s(-1000LL) % id64s(7);  /* -6 */
    check = fold64(check, (uint64_t)s);

    /* === Additional multiply stress tests === */

    /* T18: multiply 0xFFFFFFFF * 0xFFFFFFFF */
    u = id64u(0xFFFFFFFFULL) * id64u(0xFFFFFFFFULL);
    check = fold64(check, u);

    /* === Additional divide tests === */

    /* T19: unsigned divide by power of 2 */
    u = id64u(0x8000000000000000ULL) / id64u(2);
    check = fold64(check, u);

    /* T20: unsigned modulo by power of 2 */
    u = id64u(0xFFFFFFFFFFFFFFFFULL) % id64u(0x10000ULL);
    check = fold64(check, u);

    if (check == 0x3F5Au)
        halt_ok();

    fail_loop();
    return 0;
}
