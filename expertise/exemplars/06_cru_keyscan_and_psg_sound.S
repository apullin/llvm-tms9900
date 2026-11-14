; Exemplar 06: Direct TMS9901 CRU Joystick/Keyboard Scan & SN76489 Sound Driver
; Demonstrates R12 = 2*CRU_bit, LDCR/STCR 1..8 high-byte lane rule, and PSG 0x8400 framing.

    .text
    .p2align 1
    .globl ex_read_joystick1_cru
    .globl ex_psg_set_tone
    .globl ex_psg_silence_all

; uint16_t ex_read_joystick1_cru(void) -> R0 low 5 bits: [Up, Down, Right, Left, Fire] (1 = pressed)
ex_read_joystick1_cru:
    LI   R12, 0x0024        ; CRU bit 18 (2 * 18 = 0x24): TMS9901 column select P0..P2
    LI   R0, 0x0600         ; Column 6 = Joystick 1 (in HIGH byte because count=3 <= 8!)
    LDCR R0, 3              ; output 3 bits to TMS9901
    LI   R12, 0x0006        ; CRU bit 3 (2 * 3 = 0x06): row inputs INT3..INT7
    STCR R0, 5              ; read 5 active-low bits into HIGH byte of R0
    INV  R0                 ; invert active-low -> 1 = pressed
    SRL  R0, 8
    ANDI R0, 0x001F
    B    *R11

; void ex_psg_set_tone(uint16_t chan [R0], uint16_t period10 [R1], uint16_t atten [R2])
ex_psg_set_tone:
    LI   R4, 0x8400
    SLA  R0, 5              ; chan << 5
    MOV  R1, R3
    ANDI R3, 0x000F
    SOC  R0, R3
    ORI  R3, 0x0080         ; 0x80 | (chan<<5) | (period & 0x0F)
    SWPB R3
    MOVB R3, *R4

    MOV  R1, R3
    SRL  R3, 4
    ANDI R3, 0x003F         ; (period >> 4) & 0x3F
    SWPB R3
    MOVB R3, *R4

    ANDI R2, 0x000F
    SOC  R0, R2
    ORI  R2, 0x0090         ; 0x90 | (chan<<5) | (atten & 0x0F)
    SWPB R2
    MOVB R2, *R4
    B    *R11

; void ex_psg_silence_all(void)
ex_psg_silence_all:
    LI   R0, 0x8400
    LI   R1, 0x9FBF
    MOVB R1, *R0
    SWPB R1
    MOVB R1, *R0
    LI   R1, 0xDFFF
    MOVB R1, *R0
    SWPB R1
    MOVB R1, *R0
    B    *R11
