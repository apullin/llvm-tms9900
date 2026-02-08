/* 32-bit arithmetic torture test for TMS9900
 *
 * Exercises the compiler's 32-bit lowering on a 16-bit CPU:
 *   - Addition/subtraction with carry/borrow propagation
 *   - Multiplication (uses MPY 16x16->32)
 *   - Left/right shifts by various amounts (1, 8, 15, 16, 24)
 *   - Signed arithmetic right shifts
 *   - Bitwise AND, OR, XOR
 *   - Mixed 16/32-bit operations (zero-extend, sign-extend, truncate)
 *   - Unsigned and signed 32-bit comparisons
 *   - Multi-step computation chains
 */
#include <stdint.h>

__attribute__((noinline)) static void halt_ok(void) { __asm__ volatile("idle"); }
__attribute__((noinline)) static void fail_loop(void) { for (;;) {} }

/* Prevent constant folding — optnone blocks IPSCCP from seeing through */
__attribute__((noinline, optnone)) static uint32_t id32(uint32_t x) { return x; }
__attribute__((noinline, optnone)) static uint16_t id16(uint16_t x) { return x; }

/* Accumulate both halves of a 32-bit result into a 16-bit checksum */
__attribute__((noinline)) static unsigned int fold32(unsigned int check, uint32_t val) {
    check ^= (unsigned int)(val >> 16);
    check ^= (unsigned int)(val & 0xFFFFUL);
    return check;
}

int main(void) {
    unsigned int check = 0;
    uint32_t c;
    int32_t sc;

    /* T1: addition with carry propagation (0xFFFF + 1 = 0x10000) */
    c = id32(0x0000FFFFUL) + id32(0x00000001UL);
    check = fold32(check, c);

    /* T2: subtraction with borrow (0x10000 - 1 = 0xFFFF) */
    c = id32(0x00010000UL) - id32(0x00000001UL);
    check = fold32(check, c);

    /* T3: large addition crossing word boundary */
    c = id32(0x12345678UL) + id32(0xABCD0000UL);
    check = fold32(check, c);

    /* T4: 16x16 multiply (fits MPY) */
    c = id32(0x00001234UL) * id32(0x00005678UL);
    check = fold32(check, c);

    /* T5: full 32-bit multiply (requires multi-word) */
    c = id32(0x00010001UL) * id32(0x00010001UL);
    check = fold32(check, c);

    /* T6: left shift by 1 (carry across word boundary) */
    c = id32(0x80008000UL) << 1;
    check = fold32(check, c);

    /* T7: left shift by 8 */
    c = id32(0x00FF00FFUL) << 8;
    check = fold32(check, c);

    /* T8: left shift by 16 (low word becomes high) */
    c = id32(0x0000ABCDUL) << 16;
    check = fold32(check, c);

    /* T9: logical right shift by 1 */
    c = id32(0x80000001UL) >> 1;
    check = fold32(check, c);

    /* T10: logical right shift by 15 */
    c = id32(0xFFFF0000UL) >> 15;
    check = fold32(check, c);

    /* T11: logical right shift by 24 */
    c = id32(0xAB000000UL) >> 24;
    check = fold32(check, c);

    /* T12: arithmetic right shift by 1 (sign extension) */
    sc = (int32_t)id32(0x80000000UL) >> 1;
    check = fold32(check, (uint32_t)sc);

    /* T13: arithmetic right shift by 16 */
    sc = (int32_t)id32(0xFF000000UL) >> 16;
    check = fold32(check, (uint32_t)sc);

    /* T14: bitwise AND */
    c = id32(0xFF00FF00UL) & id32(0x0FF00FF0UL);
    check = fold32(check, c);

    /* T15: bitwise OR */
    c = id32(0xFF00FF00UL) | id32(0x0FF00FF0UL);
    check = fold32(check, c);

    /* T16: bitwise XOR */
    c = id32(0x12345678UL) ^ id32(0xFFFFFFFFUL);
    check = fold32(check, c);

    /* T17: zero-extend 16->32 */
    {
        uint16_t a = id16(0xABCDu);
        c = (uint32_t)a;
        check = fold32(check, c);
    }

    /* T18: sign-extend 16->32 (negative value) */
    {
        int16_t a = (int16_t)id16(0x8000u);
        sc = (int32_t)a;
        check = fold32(check, (uint32_t)sc);
    }

    /* T19: truncate 32->16 */
    {
        unsigned int tc = (unsigned int)id32(0xDEADBEEFUL);
        check ^= tc;
    }

    /* T20: unsigned 32-bit comparison (greater) */
    {
        unsigned int r = (id32(0x00010000UL) > id32(0x0000FFFFUL)) ? 1u : 0u;
        check ^= r;
    }

    /* T21: unsigned 32-bit comparison (equal low, differ high) */
    {
        unsigned int r = (id32(0x00010002UL) > id32(0x00010001UL)) ? 1u : 0u;
        check ^= r;
    }

    /* T22: signed 32-bit comparison (negative < positive) */
    {
        unsigned int r = ((int32_t)id32(0x80000000UL) < (int32_t)id32(0x00000001UL)) ? 1u : 0u;
        check ^= r;
    }

    /* T23: signed 32-bit comparison (both negative) */
    {
        unsigned int r = ((int32_t)id32(0xFFFFFFFEUL) > (int32_t)id32(0xFFFFFFFDUL)) ? 1u : 0u;
        check ^= r;
    }

    /* T24: negation (0 - x) */
    {
        uint32_t x = id32(0x00000001UL);
        c = (uint32_t)(-(int32_t)x);
        check = fold32(check, c);
    }

    /* T25: complement (~x) */
    c = ~id32(0x55AA55AAUL);
    check = fold32(check, c);

    /* T26: chain: ((a + b) * c) >> 8 */
    {
        uint32_t a = id32(0x00000100UL);
        uint32_t b = id32(0x00000200UL);
        uint32_t d = id32(0x00000003UL);
        c = ((a + b) * d) >> 8;
        check = fold32(check, c);
    }

    /* T27: chain: (a << 4) | (b >> 12) — bit field packing */
    {
        uint32_t a = id32(0x0000000FUL);
        uint32_t b = id32(0x000FF000UL);
        c = (a << 4) | (b >> 12);
        check = fold32(check, c);
    }

    /* T28: wrap-around: max + 1 = 0 */
    c = id32(0xFFFFFFFFUL) + id32(0x00000001UL);
    check = fold32(check, c);

    /* T29: multiply by zero */
    c = id32(0x12345678UL) * id32(0x00000000UL);
    check = fold32(check, c);

    /* T30: left shift by 24 */
    c = id32(0x000000ABUL) << 24;
    check = fold32(check, c);

    if (check == 0x40C2u)
        halt_ok();

    fail_loop();
    return 0;
}
