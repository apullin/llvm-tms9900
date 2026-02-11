/* String utility functions for TMS9900 freestanding runtime.
 * strlen, strcmp, strncmp, memcmp
 * Compiled with -fno-builtin to avoid recursive expansion.
 */

typedef __SIZE_TYPE__ size_t;

size_t strlen(const char *s) {
    const char *p = s;
    while (*p)
        p++;
    return (size_t)(p - s);
}

int strcmp(const char *s1, const char *s2) {
    while (*s1 && *s1 == *s2) {
        s1++;
        s2++;
    }
    return (int)(unsigned char)*s1 - (int)(unsigned char)*s2;
}

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

int memcmp(const void *s1, const void *s2, size_t n) {
    const unsigned char *p1 = (const unsigned char *)s1;
    const unsigned char *p2 = (const unsigned char *)s2;
    while (n--) {
        if (*p1 != *p2)
            return (int)*p1 - (int)*p2;
        p1++;
        p2++;
    }
    return 0;
}
