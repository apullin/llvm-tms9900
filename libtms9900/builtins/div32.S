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

       .text
       .global __udivsi3
       .global __umodsi3
       .global __divsi3
       .global __modsi3
       .global UDIV32
       .global NEG32_01
       .global NEG32_23

;----------------------------------------------------------------------
; __udivsi3 - Unsigned 32-bit division
;----------------------------------------------------------------------
__udivsi3:
       DECT R10
       MOV  R11,*R10
       BL   @UDIV32
       MOV  R4,R0
       MOV  R5,R1
       MOV  *R10+,R11
       B    *R11

;----------------------------------------------------------------------
; __umodsi3 - Unsigned 32-bit remainder
;----------------------------------------------------------------------
__umodsi3:
       DECT R10
       MOV  R11,*R10
       BL   @UDIV32
       MOV  R6,R0
       MOV  R7,R1
       MOV  *R10+,R11
       B    *R11

;----------------------------------------------------------------------
; __divsi3 - Signed 32-bit division (R9 is caller-saved, preserved by UDIV32)
;----------------------------------------------------------------------
__divsi3:
       DECT R10
       MOV  R11,*R10
       CLR  R9                ; R9 = sign flag (0 = positive)

       MOV  R0,R0
       JLT  DIV_NEG_A
DIV_CHK_B:
       MOV  R2,R2
       JLT  DIV_NEG_B
DIV_DO:
       BL   @UDIV32
       MOV  R4,R0
       MOV  R5,R1
       MOV  R9,R9
       JEQ  DIV_DONE
       INV  R1
       INV  R0
       INC  R1
       JNC  DIV_DONE
       INC  R0
DIV_DONE:
       MOV  *R10+,R11
       B    *R11

DIV_NEG_A:
       INV  R9
       INV  R1
       INV  R0
       INC  R1
       JNC  DIV_CHK_B
       INC  R0
       JMP  DIV_CHK_B

DIV_NEG_B:
       INV  R9
       INV  R3
       INV  R2
       INC  R3
       JNC  DIV_DO
       INC  R2
       JMP  DIV_DO

;----------------------------------------------------------------------
; __modsi3 - Signed 32-bit remainder (sign follows dividend)
;----------------------------------------------------------------------
__modsi3:
       DECT R10
       MOV  R11,*R10
       CLR  R9                ; R9 = dividend sign flag

       MOV  R0,R0
       JLT  MOD_NEG_A
MOD_CHK_B:
       MOV  R2,R2
       JLT  MOD_NEG_B
MOD_DO:
       BL   @UDIV32
       MOV  R6,R0
       MOV  R7,R1
       MOV  R9,R9
       JEQ  MOD_DONE
       INV  R1
       INV  R0
       INC  R1
       JNC  MOD_DONE
       INC  R0
MOD_DONE:
       MOV  *R10+,R11
       B    *R11

MOD_NEG_A:
       SETO R9
       INV  R1
       INV  R0
       INC  R1
       JNC  MOD_CHK_B
       INC  R0
       JMP  MOD_CHK_B

MOD_NEG_B:
       INV  R3
       INV  R2
       INC  R3
       JNC  MOD_DO
       INC  R2
       JMP  MOD_DO

;----------------------------------------------------------------------
; UDIV32 - Core unsigned 32-bit division
; Input:  R0:R1 = dividend
;         R2:R3 = divisor
; Output: R4:R5 = quotient
;         R6:R7 = remainder
; Clobbers: R0, R1, R8 (preserves R9..R15, zero stack usage)
;----------------------------------------------------------------------
UDIV32:
       MOV  R2,R2
       JNE  UDIV32_BIG_DIV    ; divisor >= 2^16 -> 16-step software loop
       MOV  R3,R3
       JEQ  UDIV32_DIV0       ; divisor == 0

; 16-bit divisor in R3:
       C    R0,R3
       JHE  UDIV32_TWO_DIV    ; dividend_hi >= divisor -> 2 hardware DIVs

; 1-DIV fast path (quotient < 2^16):
       DIV  R3,R0             ; R0 = quot_lo, R1 = rem_lo
       CLR  R4
       MOV  R0,R5
       CLR  R6
       MOV  R1,R7
       B    *R11

; 2-DIV fast path (arbitrary 32-bit dividend / 16-bit divisor):
UDIV32_TWO_DIV:
       CLR  R4                ; R4:R5 = 0 : dividend_hi (< R3, cannot overflow)
       MOV  R0,R5
       DIV  R3,R4             ; R4 = quot_hi, R5 = rem_hi (< R3)
       MOV  R5,R0             ; R0:R1 = rem_hi : dividend_lo (< R3, cannot overflow)
       DIV  R3,R0             ; R0 = quot_lo, R1 = final rem_lo
       MOV  R0,R5
       CLR  R6
       MOV  R1,R7
       B    *R11

; Divide-by-zero fallback: quotient = 0xFFFFFFFF, remainder = dividend
UDIV32_DIV0:
       SETO R4
       SETO R5
       MOV  R0,R6
       MOV  R1,R7
       B    *R11

; 32-bit divisor (R2 != 0, so divisor >= 0x00010000):
; Quotient fits in 16 bits (R4 = 0) and the first 16 shift-and-subtract
; steps are guaranteed < divisor, leaving R6:R7 = 0:R0 and R5 = R1.
; Only 16 iterations remain.
UDIV32_BIG_DIV:
       CLR  R4                ; quotient_hi = 0
       MOV  R1,R5             ; low 16 bits to shift out / form quotient_lo
       CLR  R6                ; remainder_hi = 0
       MOV  R0,R7             ; remainder_lo = dividend_hi
       LI   R8,16

UDIV_LOOP16:
       SLA  R6,1
       SLA  R7,1
       JNC  UDIV_NC1
       INC  R6
UDIV_NC1:
       SLA  R5,1
       JNC  UDIV_NC2
       INC  R7
UDIV_NC2:
       C    R6,R2
       JL   UDIV_NEXT
       JH   UDIV_SUB
       C    R7,R3
       JL   UDIV_NEXT

UDIV_SUB:
       S    R3,R7
       JOC  UDIV_NB
       DEC  R6
UDIV_NB:
       S    R2,R6
       INC  R5

UDIV_NEXT:
       DEC  R8
       JNE  UDIV_LOOP16
       B    *R11

;----------------------------------------------------------------------
; NEG32_01 / NEG32_23 - Kept for external callers
;----------------------------------------------------------------------
NEG32_01:
       INV  R1
       INV  R0
       INC  R1
       JNC  NEG01_DONE
       INC  R0
NEG01_DONE:
       B    *R11

NEG32_23:
       INV  R3
       INV  R2
       INC  R3
       JNC  NEG23_DONE
       INC  R2
NEG23_DONE:
       B    *R11
