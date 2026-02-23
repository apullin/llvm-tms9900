; libtms9900 - Checked 32-bit add and subtract
;
; __addvsi3: Signed 32-bit add, abort on overflow
; __subvsi3: Signed 32-bit subtract, abort on overflow
;
; Input:  R0:R1 = a (32-bit signed), R2:R3 = b (32-bit signed)
; Output: R0:R1 = a + b (or a - b)
; Effect: calls abort on signed overflow
;
; Overflow detection for add:
;   Overflow occurs when operands have SAME sign and result has DIFFERENT sign.
;   same_sign  = ~(a_hi ^ b_hi)     — bit 15 set when same sign
;   sign_flip  = a_hi ^ result_hi   — bit 15 set when sign changed
;   overflow   = same_sign & sign_flip
;
;   Using SZC: SZC src, dst computes dst &= ~src
;   R5 = a^b, R4 = a^result
;   SZC R5, R4 → R4 = (a^result) & ~(a^b) = sign_flip & same_sign = overflow

       .text
       .global __addvsi3
       .global __subvsi3

;----------------------------------------------------------------------
; __addvsi3 - Checked signed 32-bit add
;----------------------------------------------------------------------
__addvsi3:
       MOV  R0,R4              ; R4 = a_high (save for overflow check)
; Compute s = a + b
       A    R3,R1              ; add low words
       JNC  ADDV_NC
       INC  R0                 ; carry to high
ADDV_NC:
       A    R2,R0              ; add high words; R0 = result_high
; Overflow check
       MOV  R4,R5
       XOR  R2,R5              ; R5 = a_hi ^ b_hi
       XOR  R0,R4              ; R4 = a_hi ^ result_hi
       SZC  R5,R4              ; R4 = (a^result) & ~(a^b) = overflow mask
       MOV  R4,R4
       JLT  ADDV_ABORT         ; bit 15 set -> overflow
       B    *R11
ADDV_ABORT:
       B    @abort

;----------------------------------------------------------------------
; __subvsi3 - Checked signed 32-bit subtract
;
; For subtraction: overflow occurs when operands have DIFFERENT signs
; and result sign differs from a.
;   diff_sign  = a_hi ^ b_hi        — bit 15 set when different sign
;   sign_flip  = a_hi ^ result_hi   — bit 15 set when sign changed
;   overflow   = diff_sign & sign_flip
;
;   R5 = a^b, R4 = a^result
;   INV R5 → ~(a^b)
;   SZC R5, R4 → R4 = (a^result) & ~(~(a^b)) = (a^result) & (a^b) = overflow
;----------------------------------------------------------------------
__subvsi3:
       MOV  R0,R4              ; R4 = a_high
; Compute s = a - b
       S    R3,R1              ; subtract low words
       JOC  SUBV_NB            ; carry set = no borrow
       DEC  R0                 ; borrow from high
SUBV_NB:
       S    R2,R0              ; subtract high words
; Overflow check
       MOV  R4,R5
       XOR  R2,R5              ; R5 = a_hi ^ b_hi
       XOR  R0,R4              ; R4 = a_hi ^ result_hi
; overflow = (a^b) & (a^result)
; INV R5 to get ~(a^b), then SZC ~(a^b), R4 → R4 & (a^b)
       INV  R5                 ; R5 = ~(a^b)
       SZC  R5,R4              ; R4 = (a^result) & ~(~(a^b)) = (a^result) & (a^b)
       MOV  R4,R4
       JLT  SUBV_ABORT         ; bit 15 set -> overflow
       B    *R11
SUBV_ABORT:
       B    @abort
