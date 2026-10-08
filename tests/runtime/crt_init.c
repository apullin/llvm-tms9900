#include <stdint.h>

#ifndef SECTION_BYTES
#define SECTION_BYTES 1
#endif

/* Deliberately odd-sized byte sections expose startup code that assumes a
 * word-aligned end. The ROM source is preceded by an odd-sized rodata section. */
const unsigned char rodata[3] = {0x12, 0x34, 0x56};
#if SECTION_BYTES > 0
volatile unsigned char initialized[SECTION_BYTES] = {0xa5};
volatile unsigned char zeroed[SECTION_BYTES];
#endif
extern unsigned char __bss_end;

int check_startup(void) {
    unsigned sp;
    __asm__ volatile("MOV R10,%0" : "=r"(sp));
    if ((sp & 3u) != 0)
        return 1;
#if SECTION_BYTES > 0
    if (initialized[0] != 0xa5)
        return 2;
    for (unsigned i = 1; i < SECTION_BYTES; ++i)
        if (initialized[i] != 0)
            return 3;
    for (unsigned i = 0; i < SECTION_BYTES; ++i)
        if (zeroed[i] != 0)
            return 4;
#endif
    /* The runner poisons BSS and installs a canary immediately after it. */
    if (*(volatile unsigned char *)&__bss_end != 0x3c)
        return 7;

    /* Split i32 stack accesses rely on the ABI's four-byte stack boundary. */
    volatile uint32_t word = UINT32_C(0x12345678);
    if (word != UINT32_C(0x12345678))
        return 5;
    return rodata[0] == 0x12 && rodata[2] == 0x56 ? 0 : 6;
}
