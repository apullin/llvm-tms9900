/* Additional string/memory utility functions for TMS9900 freestanding runtime.
 * strlen, strcmp, and memcmp live in hand-tuned string_ops.S.
 * Compiled with -fno-builtin to avoid recursive expansion.
 */

typedef __SIZE_TYPE__ size_t;

int strncmp(const char *s1, const char *s2, size_t n) {
    while (n && *s1 && *s1 == *s2) {
        s1++;
        s2++;
        n--;
    }
    if (n == 0)
        return 0;
    return (int)(unsigned char)*s1 - (int)(unsigned char)*s2;
}

void *memchr(const void *s, int c, size_t n) {
    const unsigned char *p = (const unsigned char *)s;
    unsigned char uc = (unsigned char)c;
    while (n--) {
        if (*p == uc)
            return (void *)p;
        p++;
    }
    return (void *)0;
}
