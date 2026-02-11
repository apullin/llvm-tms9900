/* Linker script for TMS9900 benchmarks.
 * Code loads at 0x0000, workspace at 0xF000, stack at 0xFFFE.
 */
ENTRY(_start)
SECTIONS {
  . = 0x0000;
  .text : { *(.text*) }
  .rodata : { *(.rodata*) }

  .init_array : {
    . = ALIGN(2);
    PROVIDE_HIDDEN(__init_array_start = .);
    KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*) SORT_BY_INIT_PRIORITY(.ctors.*)))
    KEEP(*(.init_array .ctors))
    PROVIDE_HIDDEN(__init_array_end = .);
  }

  .data : { *(.data*) }
  .bss : { *(.bss*) *(COMMON) }
  _end = .;

  PROVIDE(__heap_start = _end);
  PROVIDE(__heap_end = 0xEFFE);
}
