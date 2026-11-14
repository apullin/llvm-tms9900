; Exemplar 07: Fast Byte-Stream Decompressors (PackBits RLE & LZ Sliding-Window)
; Demonstrates signed-byte header decoding via MOVB + SRA 8 and dual *R+ byte streaming.

    .text
    .p2align 1
    .globl ex_rle_decompress
    .globl ex_lz_decompress

; uint16_t ex_rle_decompress(const uint8_t *src [R0], uint16_t src_len [R1], uint8_t *dst [R2])
ex_rle_decompress:
    MOV  R2, R3             ; save initial dst for output length calculation
    A    R0, R1             ; R1 = src_end
.Lrle_loop:
    C    R0, R1
    JHE  .Lrle_done
    MOVB *R0+, R4           ; sets signed flags directly from header byte!
    JLT  .Lrle_neg
    SRL  R4, 8
    INC  R4                 ; count = hdr + 1 (1..128)
.Lrle_lit:
    MOVB *R0+, *R2+         ; 1-word dual-auto-increment byte copy!
    DEC  R4
    JNE  .Lrle_lit
    JMP  .Lrle_loop

.Lrle_neg:
    SRA  R4, 8              ; sign-extend hdr (-128..-1)
    CI   R4, -128
    JEQ  .Lrle_loop         ; 0x80 = PackBits no-op
    NEG  R4
    INC  R4                 ; count = 1 - hdr (2..128)
    MOVB *R0+, R5           ; repeated byte stays in high byte of R5
.Lrle_run:
    MOVB R5, *R2+
    DEC  R4
    JNE  .Lrle_run
    JMP  .Lrle_loop

.Lrle_done:
    MOV  R2, R0
    S    R3, R0
    B    *R11

; uint16_t ex_lz_decompress(const uint8_t *src [R0], uint16_t src_len [R1], uint8_t *dst [R2])
ex_lz_decompress:
    MOV  R2, R3             ; save initial dst
    A    R0, R1             ; R1 = src_end
.Llz_loop:
    C    R0, R1
    JHE  .Llz_done
    MOVB *R0+, R4           ; cmd byte (sets EQ iff cmd == 0!)
    JEQ  .Llz_lit
    SRL  R4, 8              ; R4 = copy length (1..255)
    MOVB *R0+, R5
    SRL  R5, 8              ; R5 = backward offset (1..255)
    MOV  R2, R6
    S    R5, R6             ; R6 = dst - offset
.Llz_copy:
    MOVB *R6+, *R2+         ; forward byte-by-byte supports overlapping RLE runs (offset < len)!
    DEC  R4
    JNE  .Llz_copy
    JMP  .Llz_loop

.Llz_lit:
    MOVB *R0+, *R2+
    JMP  .Llz_loop

.Llz_done:
    MOV  R2, R0
    S    R3, R0
    B    *R11
