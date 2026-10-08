/* Bump allocator for freestanding TMS9900 programs.
 * Uses __heap_start / __heap_end symbols from linker script.
 * free() is a no-op — sufficient for testing and many embedded use cases.
 */

#include <stddef.h>
#include <stdint.h>

extern char __heap_start;
extern char __heap_end;

static uintptr_t heap_ptr = (uintptr_t)&__heap_start;

void *malloc(size_t size) {
    /* Linker symbols describe an address range, not a C array. Check integer
     * bounds before any addition; 16-bit sizes and addresses can wrap. */
    uintptr_t p = heap_ptr;
    uintptr_t end = (uintptr_t)&__heap_end;
    if (p > end)
        return NULL;

    /* Two-byte alignment is sufficient for every scalar in the target ABI.
     * Also align an odd linker-supplied heap start without wrapping. */
    if (p & 1u) {
        if (p == end)
            return NULL;
        ++p;
    }
    if (size == SIZE_MAX)
        return NULL;
    size = (size + 1u) & ~(size_t)1;
    if (size > end - p)
        return NULL;

    heap_ptr = p + size;
    return (void *)p;
}

void free(void *p) {
    (void)p; /* no-op */
}
