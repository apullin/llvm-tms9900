/*
 * TI-99/4A Cartridge Linker Script
 * For systems with 32KB memory expansion
 */

/* Override with ld.lld --defsym=__stack_size=0x1000, for example. This is a
 * link-time reservation, not a runtime stack-overflow detector. */
PROVIDE(__stack_size = 0x800);
ASSERT(__stack_size >= 4 && __stack_size < 0x6000,
       "stack reservation must be between 4 bytes and 24 KiB")
ASSERT((__stack_size & 3) == 0, "stack reservation must be four-byte aligned")

MEMORY {
    /* Cartridge ROM: 8K at 0x6000 */
    ROM (rx) : ORIGIN = 0x6000, LENGTH = 0x2000

    /* Upper memory expansion: 24K at 0xA000
     * This is undisturbed RAM on systems with 32KB expansion
     * Static storage and heap end before the separately reserved C stack.
     */
    RAM (rwx) : ORIGIN = 0xA000, LENGTH = 0x6000 - __stack_size
    STACK (rw) : ORIGIN = 0x10000 - __stack_size, LENGTH = __stack_size
}

__stack_top = ORIGIN(STACK) + LENGTH(STACK) - 4;

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

    /* Discard unneeded sections */
    /DISCARD/ : {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
    }
}
