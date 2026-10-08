/*
 * Linker script for FreeRTOS on TMS9900 (flat RAM, tms9900-trace)
 *
 * Memory map:
 *   0x0000-0x003F  Interrupt vectors
 *   0x0040-0x005F  ISR workspace
 *   0x0060-0x00FF  ISR stack
 *   0x0100-0x7EFF  Code, data, BSS, statically allocated task stacks
 *   0x7F00-0x7F07  Demo result markers
 *   0x8300-0x831F  Main workspace
 *   0xE000-0xEFFF  Main C stack (grows down from 0xF000)
 *
 * This is a standalone CPU simulator layout, not a TI-99/4A hardware map.
 * Keep program storage below the markers rather than allowing growth into
 * the workspaces or main stack. The remaining flat RAM is deliberately unused.
 */

ENTRY(_start)

MEMORY
{
    VECTORS (r) : ORIGIN = 0x0000, LENGTH = 0x0040
    ISR (rw) : ORIGIN = 0x0040, LENGTH = 0x00C0
    IMAGE (rwx) : ORIGIN = 0x0100, LENGTH = 0x7E00
    MARKERS (rw) : ORIGIN = 0x7F00, LENGTH = 8
    MAIN_WORKSPACE (rw) : ORIGIN = 0x8300, LENGTH = 32
    MAIN_STACK (rw) : ORIGIN = 0xE000, LENGTH = 0x1000
}

SECTIONS
{
    /* Interrupt vector table at address 0 */
    .vectors 0x0000 : {
        KEEP(*(.vectors))
    } > VECTORS
    ASSERT(SIZEOF(.vectors) == 0x40, "interrupt vector table must be 64 bytes")

    /* Startup code first, at 0x0100 (past vectors + ISR workspace/stack) */
    .startup 0x0100 : {
        *(.startup)
    } > IMAGE

    .text : {
        *(.text)
        *(.text.*)
    } > IMAGE

    .rodata : {
        *(.rodata)
        *(.rodata.*)
    } > IMAGE

    .data : {
        *(.data)
        *(.data.*)
    } > IMAGE

    .bss (NOLOAD) : {
        __bss_start = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        __bss_end = .;
    } > IMAGE

    /* Reserve CPU register files, ISR stack, result markers and main stack;
     * these are not part of the BSS that startup clears. */
    .isr (NOLOAD) : {
        . += LENGTH(ISR);
    } > ISR
    .markers (NOLOAD) : {
        . += LENGTH(MARKERS);
    } > MARKERS
    .main_workspace (NOLOAD) : {
        . += LENGTH(MAIN_WORKSPACE);
    } > MAIN_WORKSPACE
    .main_stack (NOLOAD) : {
        . += LENGTH(MAIN_STACK);
    } > MAIN_STACK

    /* Discard unneeded sections */
    /DISCARD/ : {
        *(.comment)
        *(.note.*)
        *(.llvm_addrsig)
    }
}
