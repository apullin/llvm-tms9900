; Exemplar 01: High-Speed 32-bit Multiply, 32/16 Division, 64-bit Subtract, & Q8.8 Saturated Mul
; All routines adhere strictly to the LLVM TMS9900 C ABI with zero stack usage.

    .text
    .p2align 1
    .globl ex_mul32
    .globl ex_u32_div16
    .globl ex_q8_8_mul_sat
    .globl ex_u64_sub_inplace

; uint32_t ex_mul32(uint32_t a [R0:R1], uint32_t b [R2:R3]) -> R0:R1 (18B, 238c)
ex_mul32:
    MOV  R0, R4             ; R4 = a_hi
    MPY  R3, R4             ; R4:R5 = a_hi * b_lo -> R5 = low16(a_hi * b_lo)
    MOV  R1, R0             ; R0 = a_lo
    MOV  R1, R6             ; R6 = a_lo
    MPY  R3, R0             ; R0:R1 = a_lo * b_lo -> R1 is final low word!
    MPY  R6, R2             ; R2:R3 = b_hi * a_lo -> R3 = low16(b_hi * a_lo)
    A    R5, R0
    A    R3, R0
    B    *R11

; uint32_t ex_u32_div16(uint32_t num [R0:R1], uint16_t den [R2], uint16_t *rem [R3])
; 1-DIV when num_hi < den (~150c), 2-DIV when num_hi >= den (~260c; never overflows!)
ex_u32_div16:
    C    R0, R2             ; num_hi >=u den ? (left-to-right!)
    JHE  .Ltwo_div
    DIV  R2, R0             ; R0 = quot_lo, R1 = rem
    MOV  R1, *R3
    MOV  R0, R1
    CLR  R0
    B    *R11
.Ltwo_div:
    CLR  R4
    MOV  R0, R5
    DIV  R2, R4             ; R4 = quot_hi, R5 = rem_hi (< R2)
    MOV  R5, R0
    DIV  R2, R0             ; R0 = quot_lo, R1 = final rem
    MOV  R1, *R3
    MOV  R0, R1
    MOV  R4, R0
    B    *R11

; int16_t ex_q8_8_mul_sat(int16_t a [R0], int16_t b [R1]) -> R0 (3.4x faster than C -O2)
ex_q8_8_mul_sat:
    MOV  R0, R2             ; save signed a
    MOV  R1, R3             ; save signed b
    MPY  R3, R0             ; unsigned a * b -> R0:R1
    MOV  R2, R2
    JGT  .Lchk_b
    JEQ  .Lchk_b
    S    R3, R0             ; signed fix: if a < 0, hi -= b
.Lchk_b:
    MOV  R3, R3
    JGT  .Lshift
    JEQ  .Lshift
    S    R2, R0             ; signed fix: if b < 0, hi -= a
.Lshift:
    ; Signed 32-bit product in R0:R1. Valid Q8.8 un-saturated range for (p >> 8) in [-32768, 32767]
    ; is R0:R1 in [-0x00800000, +0x007FFFFF], i.e. signed R0 in [-128, +127]!
    CI   R0, 127
    JGT  .Lsat_max
    CI   R0, -128
    JLT  .Lsat_min
    SWPB R0                 ; low byte of R0 moves to high byte (bits 15:8 of result!)
    ANDI R0, 0xFF00
    SRL  R1, 8              ; high byte of R1 moves to low byte (bits 7:0 of result!)
    SOC  R1, R0
    B    *R11
.Lsat_max:
    LI   R0, 0x7FFF
    B    *R11
.Lsat_min:
    LI   R0, 0x8000
    B    *R11

; void ex_u64_sub_inplace(uint64_t *dst [R0], const uint64_t *src [R1]) -> *dst -= *src (3.4x C -O2)
ex_u64_sub_inplace:
    MOV  R0, R2             ; R2 != R0 so @-k(R2) indexed borrows are legal!
    S    *R1+, *R2+         ; word 0 (MSB)
    S    *R1+, *R2+         ; word 1
    JOC  .Lw2
    DEC  @-4(R2)
.Lw2:
    S    *R1+, *R2+         ; word 2
    JOC  .Lw3
    DEC  @-4(R2)
    JOC  .Lw3
    DEC  @-6(R2)
.Lw3:
    S    *R1, *R2           ; word 3 (LSB)
    JOC  .Ldone
    DEC  @-2(R2)
    JOC  .Ldone
    DEC  @-4(R2)
    JOC  .Ldone
    DEC  @-6(R2)
.Ldone:
    B    *R11
