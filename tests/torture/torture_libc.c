/* Minimal libc for GCC torture tests.
 * Provides string functions used by the test suite.
 * memcpy/memmove/memset/memcmp are in libbuiltins.a.
 */

typedef unsigned int size_t;

size_t strlen(const char *s) {
    const char *p = s;
    while (*p) p++;
    return p - s;
}

char *strcpy(char *dst, const char *src) {
    char *d = dst;
    while ((*d++ = *src++))
        ;
    return dst;
}

char *strncpy(char *dst, const char *src, size_t n) {
    char *d = dst;
    while (n && (*d++ = *src++)) n--;
    while (n--) *d++ = '\0';
    return dst;
}

int strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

int strncmp(const char *a, const char *b, size_t n) {
    while (n && *a && *a == *b) { a++; b++; n--; }
    if (!n) return 0;
    return (unsigned char)*a - (unsigned char)*b;
}

char *strcat(char *dst, const char *src) {
    char *d = dst;
    while (*d) d++;
    while ((*d++ = *src++))
        ;
    return dst;
}

char *strchr(const char *s, int c) {
    while (*s) {
        if (*s == (char)c) return (char *)s;
        s++;
    }
    return c == '\0' ? (char *)s : (void *)0;
}

char *strrchr(const char *s, int c) {
    const char *found = (void *)0;
    while (*s) {
        if (*s == (char)c) found = s;
        s++;
    }
    if (c == '\0') return (char *)s;
    return (char *)found;
}

void *calloc(size_t nmemb, size_t size) {
    /* Forward to malloc + memset. malloc is in libbuiltins.a */
    extern void *malloc(size_t);
    extern void *memset(void *, int, size_t);
    size_t total = nmemb * size;
    void *p = malloc(total);
    if (p) memset(p, 0, total);
    return p;
}

/* free() is in libbuiltins.a (malloc.c) */
