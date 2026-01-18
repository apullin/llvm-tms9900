; TI-99/4A Cartridge Startup Code (crt0.s)
; Sets up workspace, stack, clears BSS, and calls main

	.section	.cart_entry,"ax"
	.globl	_start
_start:
	; Set up workspace in scratchpad RAM
	LWPI	0x8300

	; Disable interrupts
	LIMI	0

	; Set up stack at top of upper expansion RAM
	; Stack grows downward from 0xFFFF
	LI	R10, 0xFFFE

	; Copy initialized data from ROM to RAM
	; __data_load_start is the ROM source (LMA)
	; __data_start/__data_end are the RAM destination (VMA)
	LI	R0, __data_load_start
	LI	R1, __data_start
	LI	R2, __data_end
.Ldata_loop:
	C	R1, R2
	JEQ	.Ldata_done
	MOV	*R0+, R3
	MOV	R3, *R1+
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
	MOV	R2, *R0+
	JMP	.Lbss_loop
.Lbss_done:

	; Call main (never returns)
	BL	@main

	; If main returns, loop forever
.Lhalt:
	JMP	.Lhalt
