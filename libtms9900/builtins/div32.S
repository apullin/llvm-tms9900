; libtms9900 - 32-bit division
;
; __udivsi3: Unsigned 32-bit divide
; __divsi3:  Signed 32-bit divide
; __umodsi3: Unsigned 32-bit remainder
; __modsi3:  Signed 32-bit remainder
;
; Input:  R0:R1 = dividend (high:low)
;         R2:R3 = divisor (high:low)
; Output: R0:R1 = quotient (div) or remainder (mod)
;
; Algorithm: Shift-and-subtract (restoring division)
;   We treat remainder:quotient as a 64-bit shift register.
;   For each of 32 iterations:
;     1. Shift the 64-bit value left by 1
;     2. If remainder >= divisor, subtract divisor and set quotient LSB

       .text
       .global __udivsi3
       .global __umodsi3
       .global __divsi3
       .global __modsi3

;----------------------------------------------------------------------
; __udivsi3 - Unsigned 32-bit division
; Returns quotient in R0:R1
;----------------------------------------------------------------------
__udivsi3:
       DECT R10
       MOV  R11,*R10          ; Save return address
       BL   @UDIV32           ; Do the division
       MOV  R4,R0             ; Quotient high
       MOV  R5,R1             ; Quotient low
       MOV  *R10+,R11
       B    *R11

;----------------------------------------------------------------------
; __umodsi3 - Unsigned 32-bit remainder
; Returns remainder in R0:R1
;----------------------------------------------------------------------
__umodsi3:
       DECT R10
       MOV  R11,*R10          ; Save return address
       BL   @UDIV32           ; Do the division
       MOV  R6,R0             ; Remainder high
       MOV  R7,R1             ; Remainder low
       MOV  *R10+,R11
       B    *R11

;----------------------------------------------------------------------
; __divsi3 - Signed 32-bit division
; Returns quotient in R0:R1
;----------------------------------------------------------------------
__divsi3:
       DECT R10
       MOV  R11,*R10
       DECT R10
       MOV  R9,*R10           ; Save R9 for sign flag

       CLR  R9                ; R9 = sign flag (0 = positive result)

; Check if dividend is negative
       MOV  R0,R0
       JGT  DIV_D1            ; Skip if positive
       JEQ  DIV_D1            ; Skip if zero
       INV  R9                ; Flip sign flag
       BL   @NEG32_01         ; Negate R0:R1
DIV_D1:
; Check if divisor is negative
       MOV  R2,R2
       JGT  DIV_D2            ; Skip if positive
       JEQ  DIV_D2            ; Skip if zero
       INV  R9                ; Flip sign flag
       BL   @NEG32_23         ; Negate R2:R3
DIV_D2:
; Do unsigned division
       BL   @UDIV32
       MOV  R4,R0             ; Quotient to R0:R1
       MOV  R5,R1

; Apply sign to result
       MOV  R9,R9
       JEQ  DIV_D3
       BL   @NEG32_01         ; Negate result if signs differed
DIV_D3:
       MOV  *R10+,R9
       MOV  *R10+,R11
       B    *R11

;----------------------------------------------------------------------
; __modsi3 - Signed 32-bit remainder
; Returns remainder in R0:R1
; Remainder has same sign as dividend (C99 semantics)
;----------------------------------------------------------------------
__modsi3:
       DECT R10
       MOV  R11,*R10
       DECT R10
       MOV  R9,*R10           ; Save R9 for sign flag

       CLR  R9                ; R9 = sign of dividend

; Check if dividend is negative
       MOV  R0,R0
       JGT  MOD_D1            ; Skip if positive
       JEQ  MOD_D1            ; Skip if zero
       SETO R9                ; Remember dividend was negative
       BL   @NEG32_01         ; Negate R0:R1
MOD_D1:
; Check if divisor is negative
       MOV  R2,R2
       JGT  MOD_D2            ; Skip if positive
       JEQ  MOD_D2            ; Skip if zero
       BL   @NEG32_23         ; Negate R2:R3 (don't change sign flag)
MOD_D2:
; Do unsigned division
       BL   @UDIV32
       MOV  R6,R0             ; Remainder to R0:R1
       MOV  R7,R1

; Apply dividend's sign to remainder
       MOV  R9,R9
       JEQ  MOD_D3
       BL   @NEG32_01         ; Negate remainder if dividend was negative
MOD_D3:
       MOV  *R10+,R9
       MOV  *R10+,R11
       B    *R11

;----------------------------------------------------------------------
; UDIV32 - Core unsigned division routine
; Input:  R0:R1 = dividend
;         R2:R3 = divisor
; Output: R4:R5 = quotient
;         R6:R7 = remainder
; Clobbers: R0, R1, R8
;
; Algorithm:
;   remainder = 0
;   quotient = dividend
;   for i = 0 to 31:
;     shift remainder:quotient left by 1 (64-bit shift)
;     if remainder >= divisor:
;       remainder -= divisor
;       quotient |= 1  (set LSB)
;----------------------------------------------------------------------
UDIV32:
; Fast path: if divisor fits in 16 bits, try hardware DIV
; DIV src,Rd does: Rd:Rd+1 / src -> Rd=quot, Rd+1=rem
; Precondition: Rd < src (otherwise division overflow)
       MOV  R2,R2             ; test divisor high word
       JNE  UDIV32_SW         ; nonzero -> must use software path
       MOV  R3,R3             ; test divisor low word (avoid div by zero)
       JEQ  UDIV32_SW         ; zero -> skip (div by zero handled by software path)
; Divisor is 16-bit (in R3). Check if dividend high word < divisor
; to avoid hardware DIV overflow
       C    R0,R3
       JLE  UDIV32_SW         ; R0 >= R3 -> would overflow hardware DIV
; Hardware DIV: R0:R1 / R3 -> R0=quotient, R1=remainder
       DIV  R3,R0
; Pack results: quotient in R4:R5, remainder in R6:R7
       CLR  R4                ; quotient high = 0 (fits in 16 bits)
       MOV  R0,R5             ; quotient low
       CLR  R6                ; remainder high = 0 (remainder < 16-bit divisor)
       MOV  R1,R7             ; remainder low
       B    *R11              ; return (no stack frame was created)

UDIV32_SW:
       DECT R10
       MOV  R11,*R10
       DECT R10
       MOV  R8,*R10           ; Save R8

; Initialize remainder to 0
       CLR  R6                ; Remainder high
       CLR  R7                ; Remainder low

; Initialize quotient to dividend
       MOV  R0,R4             ; Quotient high
       MOV  R1,R5             ; Quotient low

; Loop counter
       LI   R8,32             ; 32 bits to process

UDIV_LOOP:
; Shift 64-bit remainder:quotient left by 1
; Order: R6:R7:R4:R5 is the 64-bit value
;
; After shift:
;   R5_new = R5_old << 1
;   R4_new = (R4_old << 1) | (R5_old >> 15)
;   R7_new = (R7_old << 1) | (R4_old >> 15)
;   R6_new = (R6_old << 1) | (R7_old >> 15)
;
; Strategy: save MSB of each word into bit 0 of R0/R1, then shift,
; then OR the saved bits into the next higher word.
; We can reuse R0 and R1 since dividend is already copied to R4:R5.

; Save MSB of R7 (will go into R6 bit 0)
       MOV  R7,R1
       SRL  R1,15             ; R1 = old R7 bit 15 (0 or 1)
; Save MSB of R4 (will go into R7 bit 0)
       MOV  R4,R0
       SRL  R0,15             ; R0 = old R4 bit 15 (0 or 1)
; Shift R5 left; carry from R5 goes into R4
       SLA  R5,1
       JNC  UDIV_NC1
; Carry from R5 — must set R4 bit 0 AFTER shifting R4
       SLA  R4,1              ; Shift R4 (bit 0 = 0)
       INC  R4                ; Set bit 0 from R5 carry
       JMP  UDIV_R4_DONE
UDIV_NC1:
       SLA  R4,1              ; Shift R4 (no carry from R5)
UDIV_R4_DONE:
; Shift R7 left, then OR in saved R4 MSB
       SLA  R7,1
       A    R0,R7             ; Add saved bit (0 or 1) — sets R7 bit 0
; Shift R6 left, then OR in saved R7 MSB
       SLA  R6,1
       A    R1,R6             ; Add saved bit (0 or 1) — sets R6 bit 0

UDIV_CMP:
; Compare remainder (R6:R7) with divisor (R2:R3)
; Note: C Rx,Ry in LLVM assembler sets LGT when Ry > Rx (unsigned).
; So JH (LGT && !EQ) fires when Ry > Rx, and JL (!LGT && !EQ) fires when Ry < Rx.
       C    R6,R2             ; Compare high words: LGT if R2 > R6
       JH   UDIV_NEXT         ; R2 > R6: divisor_hi > rem_hi → remainder < divisor, skip
       JL   UDIV_SUB          ; R2 < R6: divisor_hi < rem_hi → remainder > divisor, subtract
       C    R7,R3             ; High equal, compare low: LGT if R3 > R7
       JH   UDIV_NEXT         ; R3 > R7: divisor_lo > rem_lo → remainder < divisor, skip

UDIV_SUB:
; Subtract divisor from remainder: R6:R7 -= R2:R3
       S    R3,R7             ; Low word
       JOC  UDIV_NB           ; Jump if no borrow (carry set = no borrow)
       DEC  R6                ; Borrow from high word
UDIV_NB:
       S    R2,R6             ; High word
; Set LSB of quotient
       ORI  R5,1

UDIV_NEXT:
       DEC  R8
       JNE  UDIV_LOOP

       MOV  *R10+,R8          ; Restore R8
       MOV  *R10+,R11
       B    *R11

;----------------------------------------------------------------------
; NEG32_01 - Negate 32-bit value in R0:R1
; Two's complement: invert all bits, add 1
;----------------------------------------------------------------------
NEG32_01:
       INV  R1                ; Invert low word
       INV  R0                ; Invert high word
       INC  R1                ; Add 1 to low
       JNC  NEG01_DONE        ; No carry, done
       INC  R0                ; Carry to high word
NEG01_DONE:
       B    *R11

;----------------------------------------------------------------------
; NEG32_23 - Negate 32-bit value in R2:R3
;----------------------------------------------------------------------
NEG32_23:
       INV  R3
       INV  R2
       INC  R3
       JNC  NEG23_DONE
       INC  R2
NEG23_DONE:
       B    *R11
