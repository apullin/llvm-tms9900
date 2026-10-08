/* The runner poisons BSS and places canaries immediately before/after it. */
#include <stdint.h>

#if SECTION_BYTES
static volatile uint8_t zeroed[SECTION_BYTES] __attribute__((aligned(1)));
#endif
extern uint8_t __bss_end[];
extern uint8_t __bss_start[];

int check_startup(void)
{
#if SECTION_BYTES
    for (unsigned i = 0; i < SECTION_BYTES; ++i)
        if (zeroed[i])
            return 1;
#endif
    if (*(volatile uint8_t *)__bss_end != 0x3c)
        return 2;
    if (*(volatile uint8_t *)((uintptr_t)__bss_start - 1) != 0x5a)
        return 3;
    return 0;
}
