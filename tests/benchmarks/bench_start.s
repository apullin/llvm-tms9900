/* Startup for benchmark programs.
 * Sets workspace pointer and stack, iterates .init_array constructors,
 * calls main(), then halts.
 */
	.text
	.globl	_start
_start:
	LWPI	0xF000
	LI	R10, 0xFFFC	; must be 4-byte aligned for ORI-based i32 access

	/* Iterate .init_array: call each global constructor */
	LI	R0, __init_array_start
	LI	R1, __init_array_end
.Linit_loop:
	C	R0, R1
	JEQ	.Linit_done
	MOV	*R0+, R2	; load function pointer, advance R0
	DECT	R10
	MOV	R0, *R10	; save R0 on stack
	DECT	R10
	MOV	R1, *R10	; save R1 on stack
	BL	*R2		; call constructor
	MOV	*R10+, R1	; restore R1
	MOV	*R10+, R0	; restore R0
	JMP	.Linit_loop
.Linit_done:

	BL	@main
.Lhalt:
	JMP	.Lhalt
