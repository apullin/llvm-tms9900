/* Startup for GCC torture tests.
 * Sets workspace pointer and stack, calls main(), halts.
 * R0 at IDLE = exit code (0 = pass, nonzero = fail).
 */
	.text
	.globl	_start
_start:
	LWPI	0xF000
	LI	R10, 0xFFFC	; 4-byte aligned SP

	CLR	R0		; default return = 0 (pass)
	BL	@main
	; Fall through: R0 = main's return value
	; (CLR R0 above covers C99 implicit return 0 from main)
.Lhalt:
	IDLE
	JMP	.Lhalt

/* void abort(void) - called on test failure */
	.globl	abort
abort:
	LI	R0, 1
	IDLE
	JMP	abort

/* void exit(int status) - R0 already has status from caller */
	.globl	exit
exit:
	; R0 = first arg = exit code, already in place
	IDLE
	JMP	exit
