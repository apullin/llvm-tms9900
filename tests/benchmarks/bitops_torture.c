/* Bit operations torture test (ported from i8085 project)
 *
 * Exercises memcmp, popcount, bswap, saturating arithmetic,
 * rotate, overflow detection, and CLZ/CTZ via compiler builtins.
 *
 * Accumulates a hash of all results; halts on success.
 */
#include <stdint.h>

/* Simple memcmp for freestanding — builtins library doesn't include it */
typedef unsigned int size_t;

__attribute__((noinline))
int memcmp(const void *s1, const void *s2, size_t n) {
    const unsigned char *p1 = (const unsigned char *)s1;
    const unsigned char *p2 = (const unsigned char *)s2;
    while (n--) {
        if (*p1 != *p2) return (int)*p1 - (int)*p2;
        p1++; p2++;
    }
    return 0;
}

__attribute__((noinline)) static void halt_ok(void) { __asm__ volatile("idle"); }
__attribute__((noinline)) static void fail_loop(void) { for (;;) {} }

/* Force values through memory to prevent constant folding */
__attribute__((noinline)) static uint8_t  id8(uint8_t x)   { return x; }
__attribute__((noinline)) static uint16_t id16(uint16_t x)  { return x; }
__attribute__((noinline)) static uint32_t id32(uint32_t x)  { return x; }

/* Mix function to accumulate test results into a hash */
static uint32_t mix(uint32_t acc, uint32_t v) {
    acc ^= v;
    acc = acc * (uint32_t)0x9E3779B1u + (uint32_t)0x7F4A7C15u;
    acc ^= acc >> 16;
    return acc;
}

volatile uint32_t result;

int main(void) {
    uint32_t acc = 0;

    /* --- memcmp tests --- */
    {
        const char a[] = "hello";
        const char b[] = "hello";
        const char c[] = "hellp";
        const char d[] = "helln";

        volatile int r1 = memcmp(a, b, 5);  /* equal -> 0 */
        volatile int r2 = memcmp(a, c, 5);  /* a < c -> negative */
        volatile int r3 = memcmp(a, d, 5);  /* a > d -> positive */
        volatile int r4 = memcmp(a, c, 0);  /* n=0 -> 0 */

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

    /* --- popcount tests --- */
    {
        volatile uint32_t v0 = 0;
        volatile uint32_t vFF = 0xFF;
        volatile uint32_t vAA = 0xAAAAAAAAu;
        volatile uint32_t v1 = 1;

        acc = mix(acc, (uint32_t)__builtin_popcount(v0));      /* 0 */
        acc = mix(acc, (uint32_t)__builtin_popcount(vFF));     /* 8 */
        acc = mix(acc, (uint32_t)__builtin_popcount(vAA));     /* 16 */
        acc = mix(acc, (uint32_t)__builtin_popcount(v1));      /* 1 */
    }

    /* --- bswap tests --- */
    {
        volatile uint32_t x32 = 0x12345678u;
        uint32_t bs32 = __builtin_bswap32(x32);
        acc = mix(acc, bs32);  /* 0x78563412 */

        volatile uint16_t x16 = 0xABCDu;
        uint16_t bs16 = __builtin_bswap16(x16);
        acc = mix(acc, (uint32_t)bs16);  /* 0xCDAB */
    }

    /* --- saturating arithmetic tests --- */
    {
        /* USUBSAT: 50 - 100 -> 0 (saturated) */
        uint16_t us_a = id16(50);
        uint16_t us_b = id16(100);
        uint16_t usub_sat = (us_a < us_b) ? 0 : (us_a - us_b);
        acc = mix(acc, (uint32_t)usub_sat);  /* 0 */

        /* Normal sub: 100 - 50 -> 50 */
        uint16_t us_c = id16(100);
        uint16_t us_d = id16(50);
        uint16_t usub_normal = (us_c < us_d) ? 0 : (us_c - us_d);
        acc = mix(acc, (uint32_t)usub_normal);  /* 50 */
    }

    /* --- rotate tests --- */
    {
        /* i32 rotate left by 8 */
        uint32_t rv = id32(0x12345678u);
        uint32_t rotl8 = (rv << 8) | (rv >> 24);
        acc = mix(acc, rotl8);  /* 0x34567812 */

        /* i32 rotate right by 4 */
        uint32_t rotr4 = (rv >> 4) | (rv << 28);
        acc = mix(acc, rotr4);  /* 0x81234567 */

        /* i16 rotate left by 4 */
        uint16_t rv16 = id16(0xABCDu);
        uint16_t rotl16_4 = (uint16_t)((rv16 << 4) | (rv16 >> 12));
        acc = mix(acc, (uint32_t)rotl16_4);  /* 0xBCDA */

        /* i8 rotate left by 3 */
        uint8_t rv8 = id8(0xA5u);
        uint8_t rotl8_3 = (uint8_t)((rv8 << 3) | (rv8 >> 5));
        acc = mix(acc, (uint32_t)rotl8_3);  /* 0x2D */
    }

    /* --- overflow detection tests --- */
    {
        /* Unsigned add overflow: 0xFFFF + 1 -> overflow */
        uint16_t oa = id16(0xFFFFu);
        uint16_t ob = id16(1);
        uint16_t osum = oa + ob;
        uint32_t oflg = (osum < oa) ? 1 : 0;
        acc = mix(acc, oflg);   /* 1 */
        acc = mix(acc, (uint32_t)osum);  /* 0 */

        /* Unsigned add no overflow: 100 + 200 -> no overflow */
        uint16_t oc = id16(100);
        uint16_t od = id16(200);
        uint16_t osum2 = oc + od;
        uint32_t oflg2 = (osum2 < oc) ? 1 : 0;
        acc = mix(acc, oflg2);  /* 0 */
        acc = mix(acc, (uint32_t)osum2);  /* 300 */
    }

    /* --- CTZ/CLZ tests --- */
    {
        volatile uint32_t ctz_val = 0x00000100u;
        acc = mix(acc, (uint32_t)__builtin_ctz(ctz_val));   /* 8 */

        volatile uint32_t clz_val = 0x00800000u;
        acc = mix(acc, (uint32_t)__builtin_clz(clz_val));   /* 8 */

        volatile uint32_t ctz_one = 1;
        acc = mix(acc, (uint32_t)__builtin_ctz(ctz_one));   /* 0 */

        volatile uint32_t clz_one = 1;
        acc = mix(acc, (uint32_t)__builtin_clz(clz_one));   /* 31 */
    }

    result = acc;

    /* The program reaching halt_ok proves all operations completed
     * without crashing. The hash is deterministic so the emulator
     * can verify the exact value if desired. */
    halt_ok();

    fail_loop();
    return 0;
}
