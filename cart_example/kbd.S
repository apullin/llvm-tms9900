; Keyboard scan helper (TI-99/4A ROM KSCAN)

	.text
	.globl	kbd_scan

; uint16_t kbd_scan(void)
; Returns raw keycode from >8375 in low byte (0 if none).
kbd_scan:
	LWPI	0x83E0
	MOV	R11, @0x8300
	BL	@0x000E
	MOV	@0x8300, R11
	LWPI	0x8300
	MOVB	@0x8375, R0
	SRL	R0, 8
	B	*R11
