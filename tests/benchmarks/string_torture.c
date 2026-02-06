/* String operations torture test (ported from i8085 project)
 *
 * Exercises strlen and strcmp with various inputs.
 * Tests correct string handling, comparisons, and edge cases.
 *
 * Expected: 12 tests pass.
 */
#include <stdint.h>

/* Simple strlen/strcmp implementations for freestanding */
typedef unsigned int size_t;

__attribute__((noinline))
size_t strlen(const char *s) {
    size_t n = 0;
    while (s[n]) n++;
    return n;
}

__attribute__((noinline))
int strcmp(const char *s1, const char *s2) {
    while (*s1 && *s1 == *s2) { s1++; s2++; }
    return (int)(unsigned char)*s1 - (int)(unsigned char)*s2;
}

#define TOTAL_TESTS 12

__attribute__((noinline)) static void halt_ok(void) { __asm__ volatile("idle"); }
__attribute__((noinline)) static void fail_loop(void) { for (;;) {} }

/* Embedded test strings */
static const char hello1[] = "Hello";
static const char hello2[] = "Hello";
static const char hellp[]  = "Hellp";
static const char hel[]    = "Hel";
static const char world[]  = "World!";
static const char empty[]  = "";

volatile uint16_t result;

int main(void) {
    volatile uint16_t pass = 0;

    /* ---- strlen tests ---- */

    /* 1. strlen("Hello") == 5 */
    if (strlen(hello1) == 5) pass++;

    /* 2. strlen("Hellp") == 5 */
    if (strlen(hellp) == 5) pass++;

    /* 3. strlen("Hel") == 3 */
    if (strlen(hel) == 3) pass++;

    /* 4. strlen("World!") == 6 */
    if (strlen(world) == 6) pass++;

    /* 5. strlen("") == 0 */
    if (strlen(empty) == 0) pass++;

    /* ---- strcmp tests ---- */

    /* 6. strcmp("Hello", "Hello") == 0 */
    if (strcmp(hello1, hello2) == 0) pass++;

    /* 7. strcmp("Hello", "Hellp") < 0  ('o' < 'p') */
    if (strcmp(hello1, hellp) < 0) pass++;

    /* 8. strcmp("Hellp", "Hello") > 0  ('p' > 'o') */
    if (strcmp(hellp, hello1) > 0) pass++;

    /* 9. strcmp("Hello", "Hel") > 0  ('l' > '\0') */
    if (strcmp(hello1, hel) > 0) pass++;

    /* 10. strcmp("Hel", "Hello") < 0  ('\0' < 'l') */
    if (strcmp(hel, hello1) < 0) pass++;

    /* 11. strcmp("Hello", "World!") < 0  ('H' < 'W') */
    if (strcmp(hello1, world) < 0) pass++;

    /* 12. strcmp("", "") == 0 */
    if (strcmp(empty, empty) == 0) pass++;

    result = pass;

    if (pass == TOTAL_TESTS)
        halt_ok();

    fail_loop();
    return 0;
}
