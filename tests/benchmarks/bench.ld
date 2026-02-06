/* Linker script for TMS9900 benchmarks.
 * Code loads at 0x0000, workspace at 0xF000, stack at 0xFFFE.
 */
ENTRY(_start)
SECTIONS {
  . = 0x0000;
  .text : { *(.text*) }
  .rodata : { *(.rodata*) }
  .data : { *(.data*) }
  .bss : { *(.bss*) *(COMMON) }
}
