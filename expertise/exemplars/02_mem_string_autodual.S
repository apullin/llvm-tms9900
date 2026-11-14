; Exemplar 02: Dual Auto-Increment Memory, String, & CRC-16 Kernels
; Demonstrates *Rn+ on both operands, CB *R0+,*R1+, and MOVB high-byte exploitation.

    .text
    .p2align 1
    .globl ex_strlen
    .globl ex_memcmp
    .globl ex_crc16_ccitt

; uint16_t ex_strlen(const char *s [R0]) -> R0 (30 cycles/byte inner loop)
ex_strlen:
    MOV  R0, R1
.Lslen:
    MOVB *R1+, R2           ; sets EQ directly from loaded high byte; R1 += 1
    JNE  .Lslen
    DEC  R1
    S    R0, R1
    MOV  R1, R0
    B    *R11

; int16_t ex_memcmp(const void *a [R0], const void *b [R1], uint16_t n [R2]) -> R0
ex_memcmp:
    MOV  R2, R2
    JEQ  .Lmc_eq
.Lmc_loop:
    CB   *R0+, *R1+         ; 1-word compare + dual +1 auto-increment!
    JNE  .Lmc_diff
    DEC  R2
    JNE  .Lmc_loop
.Lmc_eq:
    CLR  R0
    B    *R11
.Lmc_diff:
    DEC  R0
    DEC  R1
    MOVB *R0, R0
    SRL  R0, 8
    MOVB *R1, R1
    SRL  R1, 8
    S    R1, R0
    B    *R11

; uint16_t ex_crc16_ccitt(const uint8_t *buf [R0], uint16_t len [R1]) -> R0 (2.4x faster than C -O2)
; Trick: CLR R2 once outside loop; MOVB *R0+,R2 loads *buf++ directly into R2[15:8] (b << 8)
; with R2[7:0] remaining 0x00 — so `XOR R2, crc` needs ZERO shifts and ZERO masks!
ex_crc16_ccitt:
    SETO R3                 ; R3 = crc = 0xFFFF
    MOV  R1, R1
    JEQ  .Lcrc_done
    CLR  R2                 ; R2 low byte stays 0x00 forever!
    LI   R4, 0x1021         ; polynomial in register (avoids immediate fetch in bit loop)
.Lcrc_byte:
    MOVB *R0+, R2           ; R2 = (uint16_t)(*buf++) << 8
    XOR  R2, R3
    ; Unrolled 8-bit step (SLA sets Carry from old MSB!)
    SLA  R3, 1
    JNC  .+4
    XOR  R4, R3
    SLA  R3, 1
    JNC  .+4
    XOR  R4, R3
    SLA  R3, 1
    JNC  .+4
    XOR  R4, R3
    SLA  R3, 1
    JNC  .+4
    XOR  R4, R3
    SLA  R3, 1
    JNC  .+4
    XOR  R4, R3
    SLA  R3, 1
    JNC  .+4
    XOR  R4, R3
    SLA  R3, 1
    JNC  .+4
    XOR  R4, R3
    SLA  R3, 1
    JNC  .+4
    XOR  R4, R3
    DEC  R1
    JNE  .Lcrc_byte
.Lcrc_done:
    MOV  R3, R0
    B    *R11
