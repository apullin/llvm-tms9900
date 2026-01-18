/*
 * TI-99/4A Cartridge Linker Script
 * For systems with 32KB memory expansion
 */

MEMORY {
    /* Cartridge ROM: 8K at 0x6000 */
    ROM (rx) : ORIGIN = 0x6000, LENGTH = 0x2000

    /* Upper memory expansion: 24K at 0xA000
     * This is undisturbed RAM on systems with 32KB expansion
     * Stack starts at 0xFFFE and grows downward
     */
    RAM (rwx) : ORIGIN = 0xA000, LENGTH = 0x6000
}

ENTRY(_start)

SECTIONS {
    /* Header must be exactly at 0x6000 */
    .cart_header 0x6000 : {
        *(.cart_header)
    } > ROM

    /* Entry point at 0x6024 (referenced by header) */
    .cart_entry 0x6024 : {
        *(.cart_entry)
    } > ROM

    /* Code follows entry */
    .text : {
        *(.text*)
    } > ROM

    /* Read-only data (strings, etc.) */
    .rodata : {
        *(.rodata*)
    } > ROM

    /* Initialized data - in RAM */
    .data : {
        __data_start = .;
        *(.data*)
        __data_end = .;
    } > RAM AT> ROM
    __data_load_start = LOADADDR(.data);

    /* Uninitialized data */
    .bss : {
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        __bss_end = .;
    } > RAM

    /* Discard unneeded sections */
    /DISCARD/ : {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
    }
}
