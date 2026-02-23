	.text
	.globl	_start
_start:
	LWPI	0xF000
	LI	R10, 0xFFFC
	CLR	R0
	BL	@main
.Lhalt:
	IDLE
	JMP	.Lhalt

	.globl	abort
abort:
	LI	R0, 1
	IDLE
	JMP	abort

	.globl	exit
exit:
	IDLE
	JMP	exit
