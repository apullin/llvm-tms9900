; Exemplar 04: TI-99/4A Scratchpad PAD (>8320) Fast VDP Blitter, Sprite Upload, & GROM Font Loader
; Demonstrates:
;  1. VDP LSB-first vs GROM MSB-first address setup
;  2. Holding VDPWD (0x8C00) in a register so *R4 is 2B/24c (vs @0x8C00 4B/28c)
;  3. Copying an inner loop into 16-bit 0-wait Scratchpad PAD (0x8320)
;  4. VR1 BIOS shadow (0x83D4), Y-1 scanline bias, Early Clock (0x80), & 0xD0 sprite terminator

    .text
    .p2align 1
    .globl ex_vdp_upload_pad
    .globl ex_load_grom_font
    .globl ex_vdp_put_single_sprite

; void ex_vdp_upload_pad(uint16_t vram_addr [R0], const uint8_t *src [R1], uint16_t len [R2])
ex_vdp_upload_pad:
    MOV  R2, R2
    JEQ  .Lvdp_ret

    ; Set VDP write address (LSB first, (MSB & 0x3F) | 0x40 second)
    SWPB R0
    MOVB R0, @0x8C02
    SWPB R0
    ANDI R0, 0x3FFF
    ORI  R0, 0x4000
    MOVB R0, @0x8C02

    ; Copy 4-word (8-byte) relocatable byte-stream loop into 16-bit PAD at 0x8320
    LI   R3, .Lpad_loop
    LI   R4, 0x8320
    MOV  *R3+, *R4+
    MOV  *R3+, *R4+
    MOV  *R3+, *R4+
    MOV  *R3+, *R4+

    LI   R4, 0x8C00         ; VDPWD port in R4
    B    @0x8320            ; execute from 0-wait Scratchpad RAM and return via B *R11!
.Lvdp_ret:
    B    *R11

.Lpad_loop:
    MOVB *R1+, *R4          ; 2 bytes
    DEC  R2                 ; 2 bytes
    JNE  .Lpad_loop         ; 2 bytes (PC-relative — position-independent!)
    B    *R11               ; 2 bytes (total = 8 bytes in PAD!)

; void ex_load_grom_font(uint16_t grom_addr [R0], uint16_t vram_addr [R1], uint16_t num_chars [R2])
ex_load_grom_font:
    MOV  R2, R2
    JEQ  .Lgrom_ret

    ; GROM write address (0x9C02): MSB first, LSB second!
    MOVB R0, @0x9C02
    SWPB R0
    MOVB R0, @0x9C02

    ; VDP write address (0x8C02): LSB first, (MSB & 0x3F) | 0x40 second!
    SWPB R1
    MOVB R1, @0x8C02
    SWPB R1
    ANDI R1, 0x3FFF
    ORI  R1, 0x4000
    MOVB R1, @0x8C02

    LI   R4, 0x9800         ; GROMRD
    LI   R5, 0x8C00         ; VDPWD
    CLR  R6                 ; 0x00 top row
.Lglyph:
    MOVB R6, *R5            ; 1 blank top scanline
    MOVB *R4, *R5           ; 7 GROM scanlines (auto-incremented by GROM hardware!)
    MOVB *R4, *R5
    MOVB *R4, *R5
    MOVB *R4, *R5
    MOVB *R4, *R5
    MOVB *R4, *R5
    MOVB *R4, *R5
    DEC  R2
    JNE  .Lglyph
.Lgrom_ret:
    B    *R11

; void ex_vdp_put_single_sprite(uint16_t vr1 [R0], uint16_t sat [R1], int16_t y [R2],
;                               int16_t x [R3], uint16_t pat_col [*R10])
ex_vdp_put_single_sprite:
    LI   R6, 0x8C02         ; VDPWA
    LI   R7, 0x8C00         ; VDPWD

    ; Mirror VR1 to BIOS shadow 0x83D4 AND write VR1 (reg 1 -> 0x81)
    SWPB R0
    MOVB R0, @0x83D4
    MOVB R0, *R6
    LI   R4, 0x8100
    MOVB R4, *R6

    ; Set SAT VRAM write address
    SWPB R1
    MOVB R1, *R6
    SWPB R1
    ANDI R1, 0x3FFF
    ORI  R1, 0x4000
    MOVB R1, *R6

    ; Byte 0: Y - 1 (TMS9918A top-row-is-0xFF scanline offset!)
    DEC  R2
    SWPB R2
    MOVB R2, *R7

    ; Bytes 1..3: X, Pattern, EarlyClock|Color
    MOV  *R10, R4           ; high byte = pattern, low byte = color
    MOV  R3, R3             ; test signed screen_x
    JLT  .Learly_clk
    SWPB R3
    MOVB R3, *R7            ; X (0..255)
    MOVB R4, *R7            ; Pattern (already in high byte!)
    SWPB R4
    ANDI R4, 0x0F00
    MOVB R4, *R7            ; Color (EC=0)
    JMP  .Lterm
.Learly_clk:
    AI   R3, 32             ; X + 32
    SWPB R3
    MOVB R3, *R7
    MOVB R4, *R7            ; Pattern
    SWPB R4
    ANDI R4, 0x0F00
    ORI  R4, 0x8000         ; set Early Clock bit 0x80!
    MOVB R4, *R7
.Lterm:
    LI   R4, 0xD000         ; 0xD0 sprite list terminator
    MOVB R4, *R7
    B    *R11
