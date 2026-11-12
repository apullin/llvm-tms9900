/* String & memory operations torture test
 *
 * Exercises runtime strlen, strcmp (including high-bit unsigned bytes),
 * memcpy (aligned & unaligned), memset (odd bounds), and overlapping memmove.
 *
 * Expected: 18 tests pass.
 */
#include <stdint.h>

typedef __SIZE_TYPE__ size_t;

extern size_t strlen(const char *s);
extern int strcmp(const char *s1, const char *s2);
extern int memcmp(const void *s1, const void *s2, size_t n);
extern void *memcpy(void *dest, const void *src, size_t n);
extern void *memset(void *dest, int c, size_t n);
extern void *memmove(void *dest, const void *src, size_t n);

#define TOTAL_TESTS 18

__attribute__((noinline)) static void halt_ok(void) { __asm__ volatile("idle"); }
__attribute__((noinline)) static void fail_loop(void) { for (;;) {} }

static const char *opaque_str(const char *p) {
    __asm__ volatile("" : "+r"(p));
    return p;
}

static const char hello1[] = "Hello";
static const char hello2[] = "Hello";
static const char hellp[]  = "Hellp";
static const char hel[]    = "Hel";
static const char world[]  = "World!";
static const char empty[]  = "";
static const char hi_a[]   = { (char)0x80, 0 };
static const char hi_b[]   = { (char)0x7F, 0 };

volatile uint16_t result;

int main(void) {
    uint16_t pass = 0;
    uint8_t buf[24];
    uint8_t ref[24];

    /* ---- strlen tests ---- */
    if (strlen(opaque_str(hello1)) == 5) pass++;
    if (strlen(opaque_str(hellp))  == 5) pass++;
    if (strlen(opaque_str(hel))    == 3) pass++;
    if (strlen(opaque_str(world))  == 6) pass++;
    if (strlen(opaque_str(empty))  == 0) pass++;

    /* ---- strcmp tests ---- */
    if (strcmp(opaque_str(hello1), opaque_str(hello2)) == 0) pass++;
    if (strcmp(opaque_str(hello1), opaque_str(hellp))  < 0)  pass++;
    if (strcmp(opaque_str(hellp),  opaque_str(hello1)) > 0)  pass++;
    if (strcmp(opaque_str(hello1), opaque_str(hel))    > 0)  pass++;
    if (strcmp(opaque_str(hel),    opaque_str(hello1)) < 0)  pass++;
    if (strcmp(opaque_str(hello1), opaque_str(world))  < 0)  pass++;
    if (strcmp(opaque_str(empty),  opaque_str(empty))  == 0) pass++;
    /* Unsigned char comparison: 0x80 > 0x7F */
    if (strcmp(opaque_str(hi_a),   opaque_str(hi_b))   > 0)  pass++;

    /* ---- memset odd offset + odd length ---- */
    memset(buf, 0x5A, sizeof(buf));
    memset(buf + 1, 0x12A5, 7); /* must mask fill int to 0xA5 */
    if (buf[0] == 0x5A && buf[1] == 0xA5 && buf[7] == 0xA5 && buf[8] == 0x5A)
        pass++;

    /* ---- memcpy aligned and misaligned ---- */
    for (uint16_t i = 0; i < 16; i++)
        ref[i] = (uint8_t)(0x30u + i);
    memcpy(buf, ref, 11);
    if (memcmp(buf, ref, 11) == 0)
        pass++;
    memcpy(buf + 1, ref, 11);
    if (memcmp(buf + 1, ref, 11) == 0)
        pass++;

    /* ---- overlapping memmove (backward: dest > src, forward: dest < src) ---- */
    memcpy(buf, "0123456789ABCDEF", 16);
    memmove(buf + 3, buf + 1, 9); /* "012123456789CDEF" */
    if (memcmp(buf, "012123456789CDEF", 16) == 0)
        pass++;
    memmove(buf + 1, buf + 4, 8); /* "02345678989CDEF" prefix */
    if (memcmp(buf, "023456789789CDEF", 16) == 0)
        pass++;

    result = pass;

    if (pass == TOTAL_TESTS)
        halt_ok();

    fail_loop();
    return 0;
}
