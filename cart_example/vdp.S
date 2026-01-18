; VDP helper routines (TMS9918A)

	.text
	.globl	vdp_init_text
	.globl	vdp_set_register
	.globl	vdp_set_write_addr
	.globl	vdp_write_data

	.set	VDPWA, 0x8C02
	.set	VDPWD, 0x8C00
	.set	GROMA, 0x9C02
	.set	GROMD, 0x9800

; void vdp_write_data(uint16_t val_shifted)
; R0 = val (high byte)
vdp_write_data:
	MOVB	R0, @VDPWD
	B	*R11

; void vdp_set_register(uint8_t reg, uint8_t val)
; R0 = reg, R1 = val
vdp_set_register:
	ANDI	R0, 0x000F
	ORI	R0, 0x0080
	SLA	R1, 8
	ANDI	R1, 0xFF00
	SOC	R1, R0
	MOVB	R0, @VDPWA
	SWPB	R0
	MOVB	R0, @VDPWA
	B	*R11

; void vdp_set_write_addr(uint16_t addr)
; R0 = addr
vdp_set_write_addr:
	MOV	R0, R1
	SLA	R1, 8
	ANDI	R1, 0xFF00
	SRL	R0, 8
	ANDI	R0, 0x003F
	ORI	R0, 0x0040
	SOC	R1, R0
	MOVB	R0, @VDPWA
	SWPB	R0
	MOVB	R0, @VDPWA
	B	*R11

; void vdp_init_text(uint8_t color)
; R0 = color (foreground/background)
vdp_init_text:
	MOV	R11, R12
	MOV	R0, R8

	LI	R0, 0
	LI	R1, 0x00
	BL	@vdp_set_register
	LI	R0, 1
	LI	R1, 0xE0
	BL	@vdp_set_register
	LI	R0, 2
	LI	R1, 0x00
	BL	@vdp_set_register
	LI	R0, 3
	LI	R1, 0x0E
	BL	@vdp_set_register
	LI	R0, 4
	LI	R1, 0x01
	BL	@vdp_set_register
	LI	R0, 5
	LI	R1, 0x06
	BL	@vdp_set_register
	LI	R0, 6
	LI	R1, 0x00
	BL	@vdp_set_register
	LI	R0, 7
	MOV	R8, R1
	BL	@vdp_set_register

	; Initialize color table (0x0380) to match text color.
	LI	R0, 0x0380
	BL	@vdp_set_write_addr
	MOV	R8, R0
	SWPB	R0
	LI	R1, 32
color_loop:
	MOVB	R0, @VDPWD
	DEC	R1
	JNE	color_loop

	; Initialize pattern table (0x0800).
	; First 32 patterns are blank so ASCII codes map directly.
	LI	R0, 0x0800
	BL	@vdp_set_write_addr
	LI	R1, 256			; 32 * 8 bytes
	CLR	R2
blank_loop:
	MOVB	R2, @VDPWD
	DEC	R1
	JNE	blank_loop

	; Load ASCII font (GROM 0x06B4 -> VRAM 0x0900)
	LI	R0, 0x0900
	BL	@vdp_set_write_addr

	LI	R0, 0x06B4
	MOVB	R0, @GROMA
	SWPB	R0
	MOVB	R0, @GROMA

	LI	R1, 96
font_loop:
	CLR	R2
	MOVB	R2, @VDPWD
	LI	R3, 7
font_loop_row:
	MOVB	@GROMD, R2
	MOVB	R2, @VDPWD
	DEC	R3
	JNE	font_loop_row
	DEC	R1
	JNE	font_loop

	MOV	R12, R11
	B	*R11
