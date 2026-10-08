; TI-99/4A Cartridge Startup Code (crt0.s)
; Establishes workspace and stack, masks interrupts, copies .data to its RAM
; address, zeroes .bss, and calls main. C/C++ constructors are not run here.

	.section	.cart_entry,"ax"
	.globl	_start
_start:
	; Set up workspace in scratchpad RAM
	LWPI	0x8300

	; Disable interrupts
	LIMI	0

	; Set up the downward-growing stack in upper expansion RAM. The compiler
	; ABI requires a four-byte boundary, separately from word alignment.
	LI	R10, 0xFFFC

	; Copy initialized data from ROM to RAM
	; __data_load_start is the ROM source (LMA)
	; __data_start/__data_end are the RAM destination (VMA)
	LI	R0, __data_load_start
	LI	R1, __data_start
	LI	R2, __data_end
.Ldata_loop:
	C	R1, R2
	JEQ	.Ldata_done
	; Byte copies handle odd section sizes and odd ROM source addresses.
	MOVB	*R0+, R3
	MOVB	R3, *R1+
	JMP	.Ldata_loop
.Ldata_done:

	; Clear BSS section
	; __bss_start and __bss_end are defined by linker script
	LI	R0, __bss_start
	LI	R1, __bss_end
	CLR	R2
.Lbss_loop:
	C	R0, R1
	JEQ	.Lbss_done
	; Byte stores stop exactly at __bss_end, including a one-byte section.
	MOVB	R2, *R0+
	JMP	.Lbss_loop
.Lbss_done:

	; Call main (never returns)
	BL	@main

	; If main returns, loop forever
.Lhalt:
	JMP	.Lhalt
