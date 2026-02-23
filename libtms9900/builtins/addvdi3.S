; libtms9900 - Checked 64-bit add and subtract
;
; __addvdi3: Signed 64-bit add, abort on overflow
; __subvdi3: Signed 64-bit subtract, abort on overflow
;
; Input:  R0:R1:R2:R3 = a (64-bit signed)
;         Stack: b (64-bit signed)
;           *R10     = b word0 (highest)
;           @2(R10)  = b word1
;           @4(R10)  = b word2
;           @6(R10)  = b word3 (lowest)
; Output: R0:R1:R2:R3 = a + b (or a - b)
; Effect: calls abort on signed overflow

       .text
       .global __addvdi3
       .global __subvdi3

;----------------------------------------------------------------------
; __addvdi3 - Checked signed 64-bit add
;----------------------------------------------------------------------
__addvdi3:
       MOV  R0,R4              ; R4 = a_word0 (sign word, save)
       MOV  *R10,R5            ; R5 = b_word0 (sign word)
; Add from low to high with carry propagation
       A    @6(R10),R3         ; word3 += b_word3
       JNC  ADDVDI_W2
       INC  R2                 ; propagate carry to word2
       JNE  ADDVDI_W2          ; if R2 didn't wrap, stop propagating
       INC  R1                 ; propagate to word1
       JNE  ADDVDI_W2
       INC  R0                 ; propagate to word0
ADDVDI_W2:
       A    @4(R10),R2         ; word2 += b_word2
       JNC  ADDVDI_W1
       INC  R1
       JNE  ADDVDI_W1
       INC  R0
ADDVDI_W1:
       A    @2(R10),R1         ; word1 += b_word1
       JNC  ADDVDI_W0
       INC  R0
ADDVDI_W0:
       A    R5,R0              ; word0 += b_word0
; Overflow check
       MOV  R4,R6              ; R6 = a_word0
       XOR  R5,R6              ; R6 = a ^ b (sign words)
       XOR  R0,R4              ; R4 = a ^ result (sign words)
       SZC  R6,R4              ; R4 = (a^result) & ~(a^b) = overflow
       MOV  R4,R4
       JLT  ADDVDI_ABORT       ; bit 15 set -> overflow
       B    *R11
ADDVDI_ABORT:
       B    @abort

;----------------------------------------------------------------------
; __subvdi3 - Checked signed 64-bit subtract
;
; 64-bit subtract with borrow chain.
; S instruction: carry = 1 means no borrow, carry = 0 means borrow.
; DEC instruction: carry = 1 means no borrow (input was not 0).
;----------------------------------------------------------------------
__subvdi3:
       MOV  R0,R4              ; R4 = a_word0
       MOV  *R10,R5            ; R5 = b_word0
; Subtract from low to high with borrow propagation
       S    @6(R10),R3         ; word3 -= b_word3
       JOC  SUBVDI_W2          ; no borrow
       DEC  R2                 ; borrow: decrement word2
       JOC  SUBVDI_W2          ; DEC carry=1 means no wrap (input was not 0)
       DEC  R1                 ; wrap: decrement word1
       JOC  SUBVDI_W2
       DEC  R0                 ; wrap: decrement word0
SUBVDI_W2:
       S    @4(R10),R2         ; word2 -= b_word2
       JOC  SUBVDI_W1
       DEC  R1
       JOC  SUBVDI_W1
       DEC  R0
SUBVDI_W1:
       S    @2(R10),R1         ; word1 -= b_word1
       JOC  SUBVDI_W0
       DEC  R0
SUBVDI_W0:
       S    R5,R0              ; word0 -= b_word0
; Overflow check for subtraction
; overflow iff a and b have different signs AND result sign differs from a
       MOV  R4,R6
       XOR  R5,R6              ; R6 = a ^ b (bit 15: 1 if different sign)
       XOR  R0,R4              ; R4 = a ^ result
       INV  R6                 ; R6 = ~(a^b)
       SZC  R6,R4              ; R4 = (a^result) & ~(~(a^b)) = (a^result) & (a^b)
       MOV  R4,R4
       JLT  SUBVDI_ABORT
       B    *R11
SUBVDI_ABORT:
       B    @abort
