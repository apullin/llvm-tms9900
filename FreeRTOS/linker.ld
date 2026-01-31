/*
 * Linker script for FreeRTOS on TMS9900 (flat RAM, tms9900-trace)
 *
 * Memory map:
 *   0x0000-0x003F  Interrupt vectors
 *   0x0040-0x005F  ISR workspace
 *   0x0060-0x00FF  ISR stack
 *   0x0100+        Code and data
 */

ENTRY(_start)

SECTIONS
{
    /* Interrupt vector table at address 0 */
    .vectors 0x0000 : {
        *(.vectors)
    }

    /* Startup code first, at 0x0100 (past vectors + ISR workspace/stack) */
    .startup 0x0100 : {
        *(.startup)
    }

    .text : {
        *(.text)
        *(.text.*)
    }

    .rodata : {
        *(.rodata)
        *(.rodata.*)
    }

    .data : {
        *(.data)
        *(.data.*)
    }

    __bss_start = .;
    .bss : {
        *(.bss)
        *(.bss.*)
        *(COMMON)
    }
    __bss_end = .;

    /* Discard unneeded sections */
    /DISCARD/ : {
        *(.comment)
        *(.note.*)
        *(.llvm_addrsig)
    }
}
