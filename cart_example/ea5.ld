/*
 * TI-99/4A EA5 (Editor/Assembler option 5) Linker Script
 * Single-segment RAM image loaded at >A000 (requires 32KB expansion).
 *
 * The first word must be executable, so we place .cart_entry (crt0/_start)
 * at the beginning and discard any cartridge header sections.
 */

MEMORY {
    RAM (rwx) : ORIGIN = 0xA000, LENGTH = 0x6000
}

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

    /DISCARD/ : {
        *(.cart_header)
        *(.comment)
        *(.note*)
        *(.eh_frame*)
    }
}
