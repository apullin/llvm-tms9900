#include <stddef.h>
#include <stdint.h>

extern void *malloc(size_t);
extern char __heap_start, __heap_end;

int main(void) {
    static const size_t oversized[] = {0xffffu, 0xfffeu, 0x8000u, 0x7fffu, 257};
    for (unsigned i = 0; i < sizeof(oversized) / sizeof(oversized[0]); ++i)
        if (malloc(oversized[i]) != NULL)
            return 1;

    uintptr_t expected = ((uintptr_t)&__heap_start + 1u) & ~(uintptr_t)1;
    if ((uintptr_t)malloc(1) != expected)
        return 2;
    expected += 2;
    if ((uintptr_t)malloc(3) != expected)
        return 3;
    expected += 4;

    /* A failed allocation must not change the bump pointer. */
    if (malloc(0xffffu) != NULL)
        return 4;
    size_t remaining = (uintptr_t)&__heap_end - expected;
    if ((uintptr_t)malloc(remaining) != expected)
        return 5;
    if (malloc(1) != NULL)
        return 6;
    return 0;
}
