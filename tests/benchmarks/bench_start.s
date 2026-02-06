/* Minimal startup for benchmark programs.
 * Sets workspace pointer and stack, calls main(), then halts.
 */
	.text
	.globl	_start
_start:
	LWPI	0xF000
	LI	R10, 0xFFFE
	BL	@main
.Lhalt:
	JMP	.Lhalt
