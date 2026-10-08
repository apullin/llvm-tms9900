; life3_2x_step.S - Hand-coded Game of Life inner loop for TMS9900
;
; void life_compute_row(uint16_t *row_prev, uint16_t *row,
;                       uint16_t *row_next, uint16_t *row_dst)
;
; Processes one full row (8 words, 128 cells) using sliding window + CSA.
; CELL_WORDS = 8 (128 / 16).
;
; ABI: R0-R3 = params, R0-R9/R12 caller-saved, R13-R15 callee-saved
;      R10 = SP, R11 = LR
; This implementation additionally preserves R9 for its own register plan.
;
; IMPORTANT: R0 cannot be used as an index register on TMS9900.
; When Ts=10,S=0000 the CPU treats it as symbolic (absolute) addressing.
; So we move row_prev (R0 on entry) into R3 and use R0 as loop counter.
;
; Register plan:
;   R3  = row_prev pointer (moved from R0 at entry)
;   R1  = row pointer (unchanged from entry)
;   R2  = row_next pointer (unchanged from entry)
;   R0  = word byte offset for loop (words 1-6)
;   row_dst saved to stack (only needed for stores)
;   R4-R9, R12-R15 = 10 registers for CSA computation
;   mC spilled to stack once per word (1 push/pop = 56 cycles)
;   CSA tree in subroutine (.Lcsa_life) called via BL

        .text
        .globl  life_compute_row

life_compute_row:
        ; === Prologue: save callee-saved regs + row_dst ===
        DECT R10
        MOV  R11, *R10         ; save LR
        DECT R10
        MOV  R9, *R10          ; save R9
        DECT R10
        MOV  R13, *R10         ; save R13
        DECT R10
        MOV  R14, *R10         ; save R14
        DECT R10
        MOV  R15, *R10         ; save R15
        DECT R10
        MOV  R3, *R10          ; save row_dst
        ; Stack: [SP+0]=row_dst [SP+2]=R15 [SP+4]=R14 [SP+6]=R13 [SP+8]=R9 [SP+10]=LR

        ; Move row_prev from R0 to R3 (R0 can't be used as index register)
        MOV  R0, R3

        ; ============================================================
        ; WORD 0: left neighbor wraps to word 7
        ; Load: left=word7(@14), center=word0(*), right=word1(@2)
        ; ============================================================
        ; Top row
        MOV  @14(R3), R5       ; prev[7] = left_word
        MOV  *R3, R6           ; prev[0] = center_word (uC)
        MOV  @2(R3), R7        ; prev[1] = right_word

        ; u_left = (center >> 1) | (left << 15) -> R4
        MOV  R6, R4
        SRL  R4, 1
        SLA  R5, 15
        SOC  R5, R4            ; R4 = u_left

        ; u_right = (center << 1) | (right >> 15) -> R5
        MOV  R6, R5
        SLA  R5, 1
        SRL  R7, 15
        SOC  R7, R5            ; R5 = u_right
        ; R4=u_left, R5=u_right, R6=uC

        ; Middle row
        MOV  @14(R1), R8       ; row[7]
        MOV  *R1, R9           ; row[0] = mC
        MOV  @2(R1), R14       ; row[1]

        ; m_left -> R7
        MOV  R9, R7
        SRL  R7, 1
        SLA  R8, 15
        SOC  R8, R7            ; R7 = m_left

        ; m_right -> R8
        MOV  R9, R8
        SLA  R8, 1
        SRL  R14, 15
        SOC  R14, R8           ; R8 = m_right

        ; Push mC
        DECT R10
        MOV  R9, *R10

        ; Bottom row
        MOV  @14(R2), R15      ; next[7]
        MOV  *R2, R13          ; next[0] = dC
        MOV  @2(R2), R12       ; next[1]

        ; d_left -> R9
        MOV  R13, R9
        SRL  R9, 1
        SLA  R15, 15
        SOC  R15, R9           ; R9 = d_left

        ; d_right -> R14
        MOV  R13, R14
        SLA  R14, 1
        SRL  R12, 15
        SOC  R12, R14          ; R14 = d_right

        ; CSA + life rule
        BL   @.Lcsa_life       ; result in R5, pops mC

        ; Store word 0
        MOV  *R10, R4          ; row_dst
        MOV  R5, *R4           ; row_dst[0] = result

        ; ============================================================
        ; WORDS 1-6: sliding window loop
        ; R0 = word byte offset (2..12), R3 = row_prev ptr
        ; ============================================================
        LI   R0, 2             ; byte offset for word 1

.Lloop:
        ; Top row: load 3 words using auto-increment
        MOV  R3, R4
        A    R0, R4
        DECT R4                ; R4 = &prev[wx-1]
        MOV  *R4+, R5          ; prev[wx-1], R4 -> &prev[wx]
        MOV  *R4+, R6          ; prev[wx] = uC, R4 -> &prev[wx+1]
        MOV  *R4, R7           ; prev[wx+1]

        ; u_left -> R4
        MOV  R6, R4
        SRL  R4, 1
        SLA  R5, 15
        SOC  R5, R4

        ; u_right -> R5
        MOV  R6, R5
        SLA  R5, 1
        SRL  R7, 15
        SOC  R7, R5

        ; Middle row
        MOV  R1, R7
        A    R0, R7
        DECT R7                ; R7 = &row[wx-1]
        MOV  *R7+, R8          ; row[wx-1]
        MOV  *R7+, R9          ; row[wx] = mC
        MOV  *R7, R14          ; row[wx+1]

        ; m_left -> R7
        MOV  R9, R7
        SRL  R7, 1
        SLA  R8, 15
        SOC  R8, R7

        ; m_right -> R8
        MOV  R9, R8
        SLA  R8, 1
        SRL  R14, 15
        SOC  R14, R8

        ; Push mC
        DECT R10
        MOV  R9, *R10

        ; Bottom row
        MOV  R2, R12
        A    R0, R12
        DECT R12               ; R12 = &next[wx-1]
        MOV  *R12+, R15        ; next[wx-1]
        MOV  *R12+, R13        ; next[wx] = dC
        MOV  *R12, R12         ; next[wx+1]

        ; d_left -> R9
        MOV  R13, R9
        SRL  R9, 1
        SLA  R15, 15
        SOC  R15, R9

        ; d_right -> R14
        MOV  R13, R14
        SLA  R14, 1
        SRL  R12, 15
        SOC  R12, R14

        ; CSA + life rule
        BL   @.Lcsa_life       ; result in R5

        ; Store
        MOV  *R10, R4          ; row_dst
        A    R0, R4
        MOV  R5, *R4           ; row_dst[wx] = result

        ; Next word
        INCT R0
        CI   R0, 12
        JLE  .Lloop

        ; ============================================================
        ; WORD 7: right neighbor wraps to word 0
        ; Load: left=word6(@12), center=word7(@14), right=word0(*)
        ; ============================================================
        ; Top row
        MOV  @12(R3), R5       ; prev[6]
        MOV  @14(R3), R6       ; prev[7] = uC
        MOV  *R3, R7           ; prev[0] = right wrap

        MOV  R6, R4
        SRL  R4, 1
        SLA  R5, 15
        SOC  R5, R4

        MOV  R6, R5
        SLA  R5, 1
        SRL  R7, 15
        SOC  R7, R5

        ; Middle row
        MOV  @12(R1), R8       ; row[6]
        MOV  @14(R1), R9       ; row[7] = mC
        MOV  *R1, R14          ; row[0] = right wrap

        MOV  R9, R7
        SRL  R7, 1
        SLA  R8, 15
        SOC  R8, R7

        MOV  R9, R8
        SLA  R8, 1
        SRL  R14, 15
        SOC  R14, R8

        DECT R10
        MOV  R9, *R10

        ; Bottom row
        MOV  @12(R2), R15      ; next[6]
        MOV  @14(R2), R13      ; next[7] = dC
        MOV  *R2, R12          ; next[0] = right wrap

        MOV  R13, R9
        SRL  R9, 1
        SLA  R15, 15
        SOC  R15, R9

        MOV  R13, R14
        SLA  R14, 1
        SRL  R12, 15
        SOC  R12, R14

        BL   @.Lcsa_life

        ; Store word 7
        MOV  *R10, R4          ; row_dst
        MOV  R5, @14(R4)       ; row_dst[7] = result

        ; === Epilogue ===
        AI   R10, 2            ; pop row_dst
        MOV  *R10+, R15
        MOV  *R10+, R14
        MOV  *R10+, R13
        MOV  *R10+, R9
        MOV  *R10+, R11        ; restore LR
        B    *R11

; ================================================================
; CSA Tree + Life Rule subroutine
;
; Input registers (set by caller after shift computation):
;   R4  = u_left       R7  = m_left      R9  = d_left
;   R6  = uC           (mC on stack)     R13 = dC
;   R5  = u_right      R8  = m_right     R14 = d_right
;   *R10 = mC (pushed by caller)
;
; Output: R5 = life rule result
; Side effect: pops mC (R10 += 2)
; Clobbers: R4-R9, R12-R15
; Preserves: R0-R3, R10 (net), R11
; ================================================================
.Lcsa_life:
        ; --- FA1(u_left=R4, uC=R6, u_right=R5) -> sum1=R12, carry1=R5 ---
        MOV  R4, R12           ; R12 = u_left
        XOR  R6, R12           ; R12 = u_left ^ uC
        MOV  R12, R15          ; R15 = u_left ^ uC
        XOR  R5, R12           ; R12 = sum1

        INV  R4                ; ~u_left
        SZC  R4, R6            ; R6 = uC & u_left
        INV  R15               ; ~(u_left ^ uC)
        SZC  R15, R5           ; R5 = u_right & (u_left ^ uC)
        SOC  R6, R5            ; R5 = carry1

        ; --- FA2(m_left=R7, m_right=R8, d_left=R9) -> sum2=R4, carry2=R9 ---
        MOV  R7, R4            ; R4 = m_left
        XOR  R8, R4            ; R4 = m_left ^ m_right
        MOV  R4, R6            ; R6 = m_left ^ m_right
        XOR  R9, R4            ; R4 = sum2

        INV  R7                ; ~m_left
        SZC  R7, R8            ; R8 = m_right & m_left
        INV  R6                ; ~(m_left ^ m_right)
        SZC  R6, R9            ; R9 = d_left & (m_left ^ m_right)
        SOC  R8, R9            ; R9 = carry2

        ; --- HA(dC=R13, d_right=R14) -> sum3=R6, carry3=R14 ---
        MOV  R13, R6           ; R6 = dC
        XOR  R14, R6           ; R6 = sum3
        INV  R13               ; ~dC
        SZC  R13, R14          ; R14 = d_right & dC = carry3

        ; --- Stage 2: FA(sum1=R12, sum2=R4, sum3=R6) -> bit0=R7, carry4=R6 ---
        MOV  R12, R7           ; R7 = sum1
        XOR  R4, R7            ; R7 = sum1 ^ sum2
        MOV  R7, R8            ; R8 = sum1 ^ sum2
        XOR  R6, R7            ; R7 = bit0

        INV  R12               ; ~sum1
        SZC  R12, R4           ; R4 = sum2 & sum1
        INV  R8                ; ~(sum1 ^ sum2)
        SZC  R8, R6            ; R6 = sum3 & (sum1 ^ sum2)
        SOC  R4, R6            ; R6 = carry4

        ; --- Stage 2: FA(carry1=R5, carry2=R9, carry3=R14) -> t_sum=R4, t_carry=R14 ---
        MOV  R5, R4            ; R4 = carry1
        XOR  R9, R4            ; R4 = carry1 ^ carry2
        MOV  R4, R8            ; R8 = carry1 ^ carry2
        XOR  R14, R4           ; R4 = t_sum

        INV  R5                ; ~carry1
        SZC  R5, R9            ; R9 = carry2 & carry1
        INV  R8                ; ~(carry1 ^ carry2)
        SZC  R8, R14           ; R14 = carry3 & (carry1 ^ carry2)
        SOC  R9, R14           ; R14 = t_carry

        ; --- Stage 3: HA(t_sum=R4, carry4=R6) -> bit1=R5, c1=R6 ---
        MOV  R4, R5            ; R5 = t_sum
        XOR  R6, R5            ; R5 = bit1
        INV  R4                ; ~t_sum
        SZC  R4, R6            ; R6 = c1 = carry4 & t_sum

        ; --- Stage 3: HA(t_carry=R14, c1=R6) -> bit2=R4, bit3=R6 ---
        MOV  R14, R4           ; R4 = t_carry
        XOR  R6, R4            ; R4 = bit2
        INV  R14               ; ~t_carry
        SZC  R14, R6           ; R6 = bit3 = c1 & t_carry

        ; --- Life rule: bit1 & ~bit2 & ~bit3 & (bit0 | mC) ---
        ; R7=bit0, R5=bit1, R4=bit2, R6=bit3
        MOV  *R10+, R8         ; pop mC
        SOC  R8, R7            ; R7 = bit0 | mC
        SZC  R4, R5            ; R5 = bit1 & ~bit2
        SZC  R6, R5            ; R5 = bit1 & ~bit2 & ~bit3
        INV  R7                ; ~(bit0 | mC)
        SZC  R7, R5            ; R5 = result

        B    *R11              ; return to caller
