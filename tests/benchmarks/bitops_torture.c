/* Bit operations torture test
 *
 * Exercises memcmp, 16/32-bit popcount, bswap, saturating arithmetic,
 * rotate, overflow detection, and 16/32-bit CLZ/CTZ via compiler builtins.
 *
 * Accumulates a deterministic hash of all results and verifies it before halting.
 */
#include <stdint.h>

typedef __SIZE_TYPE__ size_t;
extern int memcmp(const void *s1, const void *s2, size_t n);

__attribute__((noinline)) static void halt_ok(void) { __asm__ volatile("idle"); }
__attribute__((noinline)) static void fail_loop(void) { for (;;) {} }

/* Opaque value barriers prevent compile-time constant folding across O0..Oz and LTO */
static uint8_t id8(uint8_t x) {
    uint16_t w = x;
    __asm__ volatile("" : "+r"(w));
    return (uint8_t)w;
}

static uint16_t id16(uint16_t x) {
    __asm__ volatile("" : "+r"(x));
    return x;
}

static uint32_t id32(uint32_t x) {
    uint16_t hi = (uint16_t)(x >> 16);
    uint16_t lo = (uint16_t)x;
    __asm__ volatile("" : "+r"(hi), "+r"(lo));
    return ((uint32_t)hi << 16) | (uint32_t)lo;
}

/* Mix function to accumulate test results into a hash */
static uint32_t mix(uint32_t acc, uint32_t v) {
    acc ^= v;
    acc = acc * (uint32_t)0x9E3779B1UL + (uint32_t)0x7F4A7C15UL;
    acc ^= acc >> 16;
    return acc;
}

#define EXPECTED_ACC 0xB8D09F0BUL

volatile uint32_t result;

int main(void) {
    uint32_t acc = 0;

    /* --- memcmp tests --- */
    {
        static const char a[] = "hello";
        static const char b[] = "hello";
        static const char c[] = "hellp";
        static const char d[] = "helln";
        const void *pa = a, *pb = b, *pc = c, *pd = d;
        __asm__ volatile("" : "+r"(pa), "+r"(pb), "+r"(pc), "+r"(pd));

        int r1 = memcmp(pa, pb, 5);  /* equal -> 0 */
        int r2 = memcmp(pa, pc, 5);  /* a < c -> negative */
        int r3 = memcmp(pa, pd, 5);  /* a > d -> positive */
        int r4 = memcmp(pa, pc, 0);  /* n=0 -> 0 */

        /* Normalize to -1, 0, 1 */
        int n1 = (r1 == 0) ? 0 : ((r1 < 0) ? -1 : 1);
        int n2 = (r2 == 0) ? 0 : ((r2 < 0) ? -1 : 1);
        int n3 = (r3 == 0) ? 0 : ((r3 < 0) ? -1 : 1);
        int n4 = (r4 == 0) ? 0 : ((r4 < 0) ? -1 : 1);

        acc = mix(acc, (uint32_t)(n1 + 2));  /* 2 */
        acc = mix(acc, (uint32_t)(n2 + 2));  /* 1 */
        acc = mix(acc, (uint32_t)(n3 + 2));  /* 3 */
        acc = mix(acc, (uint32_t)(n4 + 2));  /* 2 */
    }

    /* --- 32-bit and 16-bit popcount tests --- */
    {
        uint32_t v0  = id32(0UL);
        uint32_t vFF = id32(0xFFUL);
        uint32_t vAA = id32(0xAAAAAAAAUL);
        uint32_t v1  = id32(1UL);
        uint16_t w55 = id16(0x5555u);
        uint16_t w81 = id16(0x8001u);

        acc = mix(acc, (uint32_t)__builtin_popcountl(v0));   /* 0 */
        acc = mix(acc, (uint32_t)__builtin_popcountl(vFF));  /* 8 */
        acc = mix(acc, (uint32_t)__builtin_popcountl(vAA));  /* 16 */
        acc = mix(acc, (uint32_t)__builtin_popcountl(v1));   /* 1 */
        acc = mix(acc, (uint32_t)__builtin_popcount(w55));   /* 8 */
        acc = mix(acc, (uint32_t)__builtin_popcount(w81));   /* 2 */
    }

    /* --- bswap tests --- */
    {
        uint32_t x32 = id32(0x12345678UL);
        uint32_t bs32 = __builtin_bswap32(x32);
        acc = mix(acc, bs32);  /* 0x78563412 */

        uint16_t x16 = id16(0xABCDu);
        uint16_t bs16 = __builtin_bswap16(x16);
        acc = mix(acc, (uint32_t)bs16);  /* 0xCDAB */
    }

    /* --- saturating arithmetic tests --- */
    {
        uint16_t us_a = id16(50);
        uint16_t us_b = id16(100);
        uint16_t usub_sat = (us_a < us_b) ? 0 : (us_a - us_b);
        acc = mix(acc, (uint32_t)usub_sat);  /* 0 */

        uint16_t us_c = id16(100);
        uint16_t us_d = id16(50);
        uint16_t usub_normal = (us_c < us_d) ? 0 : (us_c - us_d);
        acc = mix(acc, (uint32_t)usub_normal);  /* 50 */
    }

    /* --- rotate tests --- */
    {
        uint32_t rv = id32(0x12345678UL);
        uint32_t rotl8 = (rv << 8) | (rv >> 24);
        acc = mix(acc, rotl8);  /* 0x34567812 */

        uint32_t rotr4 = (rv >> 4) | (rv << 28);
        acc = mix(acc, rotr4);  /* 0x81234567 */

        uint16_t rv16 = id16(0xABCDu);
        uint16_t rotl16_4 = (uint16_t)((rv16 << 4) | (rv16 >> 12));
        acc = mix(acc, (uint32_t)rotl16_4);  /* 0xBCDA */

        uint8_t rv8 = id8(0xA5u);
        uint8_t rotl8_3 = (uint8_t)((rv8 << 3) | (rv8 >> 5));
        acc = mix(acc, (uint32_t)rotl8_3);  /* 0x2D */
    }

    /* --- overflow detection tests --- */
    {
        uint16_t oa = id16(0xFFFFu);
        uint16_t ob = id16(1);
        uint16_t osum = oa + ob;
        uint32_t oflg = (osum < oa) ? 1 : 0;
        acc = mix(acc, oflg);            /* 1 */
        acc = mix(acc, (uint32_t)osum);  /* 0 */

        uint16_t oc = id16(100);
        uint16_t od = id16(200);
        uint16_t osum2 = oc + od;
        uint32_t oflg2 = (osum2 < oc) ? 1 : 0;
        acc = mix(acc, oflg2);            /* 0 */
        acc = mix(acc, (uint32_t)osum2);  /* 300 */
    }

    /* --- 32-bit and 16-bit CTZ/CLZ tests (non-zero inputs only) --- */
    {
        acc = mix(acc, (uint32_t)__builtin_ctzl(id32(0x00000100UL)));  /* 8 */
        acc = mix(acc, (uint32_t)__builtin_clzl(id32(0x00800000UL)));  /* 8 */
        acc = mix(acc, (uint32_t)__builtin_ctzl(id32(1UL)));           /* 0 */
        acc = mix(acc, (uint32_t)__builtin_clzl(id32(1UL)));           /* 31 */
        acc = mix(acc, (uint32_t)__builtin_ctzl(id32(0x80000000UL)));  /* 31 */

        acc = mix(acc, (uint32_t)__builtin_ctz(id16(0x0100u)));        /* 8 */
        acc = mix(acc, (uint32_t)__builtin_clz(id16(0x0080u)));        /* 8 */
        acc = mix(acc, (uint32_t)__builtin_ctz(id16(1u)));             /* 0 */
        acc = mix(acc, (uint32_t)__builtin_clz(id16(1u)));             /* 15 */
    }

    result = acc;

    if (acc == EXPECTED_ACC)
        halt_ok();

    fail_loop();
    return 0;
}
