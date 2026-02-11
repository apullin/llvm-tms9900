/* Bump allocator for freestanding TMS9900 programs.
 * Uses __heap_start / __heap_end symbols from linker script.
 * free() is a no-op — sufficient for testing and many embedded use cases.
 */

typedef unsigned int size_t;

extern char __heap_start;
extern char __heap_end;

static char *heap_ptr = &__heap_start;

void *malloc(size_t size) {
    /* Align size up to 2 bytes */
    size = (size + 1u) & ~1u;

    char *p = heap_ptr;
    char *next = p + size;

    if (next > &__heap_end)
        return (void *)0;

    heap_ptr = next;
    return p;
}

void free(void *p) {
    (void)p; /* no-op */
}
