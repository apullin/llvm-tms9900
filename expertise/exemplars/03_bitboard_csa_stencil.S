; Exemplar 03: Bit-Parallel 16-Cell Conway's Game of Life Stencil Word Step
; Demonstrates Carry-Save Adder (CSA) bit-parallel neighbor counting + 1-bit Carry edge shifts
; (distilled from cart_example/life3_2x_step.S — ~15x faster than scalar neighbor loops).

    .text
    .p2align 1
    .globl ex_life_word16

; uint16_t ex_life_word16(uint16_t top [R0], uint16_t mid [R1], uint16_t bot [R2]) -> R0
; Computes next Life generation for 16 horizontal cells simultaneously (zero-padded edges;
; for toroidal neighbors, shift neighbor word by 1 into Carry and JNC + ORI 0x8000 / INC!).
ex_life_word16:
    ; Horizontal 3-input full adders for top (R0) and bot (R2), half adder for mid (R1):
    ; Left-shifted and right-shifted neighbor words:
    MOV  R0, R3
    SLA  R3, 1              ; top_l
    MOV  R0, R4
    SRL  R4, 1              ; top_r

    ; Full adder(top_l=R3, top=R0, top_r=R4) -> sum_t in R3, carry_t in R0:
    MOV  R3, R5
    XOR  R0, R3             ; R3 = a ^ b
    INV  R0
    SZC  R0, R5             ; R5 = a & b (via ~(~b) & a)
    MOV  R3, R0
    XOR  R4, R3             ; R3 = sum_t = a ^ b ^ c
    INV  R4
    SZC  R4, R0             ; R0 = (a ^ b) & c
    SOC  R5, R0             ; R0 = carry_t

    ; Full adder(bot_l, bot, bot_r) -> sum_b in R6, carry_b in R2:
    MOV  R2, R6
    SLA  R6, 1
    MOV  R2, R7
    SRL  R7, 1
    MOV  R6, R5
    XOR  R2, R6
    INV  R2
    SZC  R2, R5
    MOV  R6, R2
    XOR  R7, R6             ; R6 = sum_b
    INV  R7
    SZC  R7, R2
    SOC  R5, R2             ; R2 = carry_b

    ; Half adder(mid_l, mid_r) -> sum_m in R4, carry_m in R5 (keep center mid in R1!):
    MOV  R1, R4
    SLA  R4, 1
    MOV  R1, R5
    SRL  R5, 1
    MOV  R4, R7
    XOR  R5, R4             ; R4 = sum_m
    INV  R5
    SZC  R5, R7             ; R7 = carry_m

    ; Full adder(sum_t=R3, sum_b=R6, sum_m=R4) -> bit0 in R3, c1 in R6:
    MOV  R3, R5
    XOR  R6, R3
    INV  R6
    SZC  R6, R5
    MOV  R3, R6
    XOR  R4, R3             ; R3 = bit0 of 8-neighbor count!
    INV  R4
    SZC  R4, R6
    SOC  R5, R6             ; R6 = c1

    ; Full adder(carry_t=R0, carry_b=R2, carry_m=R7) -> s2 in R0, c2 in R2:
    MOV  R0, R5
    XOR  R2, R0
    INV  R2
    SZC  R2, R5
    MOV  R0, R2
    XOR  R7, R0             ; R0 = s2
    INV  R7
    SZC  R7, R2
    SOC  R5, R2             ; R2 = c2

    ; Simplified final rule: alive_next = (s2 ^ c1) & ~c2 & ~(s2 & c1) & (bit0 | center)
    ; Notice (s2 ^ c1) & ~(s2 & c1) == (s2 ^ c1) identically!
    ; So alive_next = (s2 ^ c1) & ~c2 & (bit0 | center)!
    XOR  R6, R0             ; R0 = bit1_raw = s2 ^ c1
    SZC  R2, R0             ; R0 &= ~c2
    SOC  R1, R3             ; R3 = bit0 | center
    INV  R3
    SZC  R3, R0             ; R0 &= (bit0 | center)
    B    *R11
