/*
 * TI-99/4A EA5 (Editor/Assembler option 5) Linker Script
 * Single-segment RAM image loaded at >A000 (requires 32KB expansion).
 *
 * The first word must be executable, so we place .cart_entry (crt0/_start)
 * at the beginning and discard any cartridge header sections.
 */

/* Match cartridge startup's stack/heap boundary. Override with
 * ld.lld --defsym=__stack_size=0x1000 when a larger C stack is required. */
PROVIDE(__stack_size = 0x800);
ASSERT(__stack_size >= 4 && __stack_size < 0x6000,
       "stack reservation must be between 4 bytes and 24 KiB")
ASSERT((__stack_size & 3) == 0, "stack reservation must be four-byte aligned")

MEMORY {
    RAM (rwx) : ORIGIN = 0xA000, LENGTH = 0x6000 - __stack_size
    STACK (rw) : ORIGIN = 0x10000 - __stack_size, LENGTH = __stack_size
}

__stack_top = ORIGIN(STACK) + LENGTH(STACK) - 4;

ENTRY(_start)

SECTIONS {
    /* Entry code must be first for EA5 */
    .text : {
        *(.cart_entry)
        *(.text*)
    } > RAM

    .rodata : {
        *(.rodata*)
    } > RAM

    .data : {
        __data_start = .;
        *(.data*)
        __data_end = .;
    } > RAM
    __data_load_start = LOADADDR(.data);

    .bss (NOLOAD) : {
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        __bss_end = .;
    } > RAM

    __heap_start = ALIGN(__bss_end, 2);
    __heap_end = ORIGIN(STACK);
    .stack (NOLOAD) : {
        . += LENGTH(STACK);
    } > STACK

    /DISCARD/ : {
        *(.cart_header)
        *(.comment)
        *(.note*)
        *(.eh_frame*)
    }
}
