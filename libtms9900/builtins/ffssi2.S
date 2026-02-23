; libtms9900 - Find first set bit operations
;
; __ffssi2: Find first set bit in 32-bit value
; __ffsdi2: Find first set bit in 64-bit value
;
; Returns: 0 if input is zero, else index of least significant 1-bit (1-based)
;          i.e., ctz(a) + 1
;
; __ffssi2:
;   Input:  R0:R1 = 32-bit value (high:low)
;   Output: R0 = bit index (0 if zero, 1-32 otherwise)
;
; __ffsdi2:
;   Input:  R0:R1:R2:R3 = 64-bit value (high to low)
;   Output: R0 = bit index (0 if zero, 1-64 otherwise)

       .text
       .global __ffssi2
       .global __ffsdi2

;----------------------------------------------------------------------
; __ffssi2 - Find first set in 32-bit value
;----------------------------------------------------------------------
__ffssi2:
; Check low word first (has the trailing bits)
       MOV  R1,R2              ; R2 = low word, sets flags
       JNE  FFS32_LO
; Low word zero, check high
       MOV  R0,R2              ; R2 = high word, sets flags
       JEQ  FFS32_ZERO         ; both zero -> return 0
       LI   R0,16              ; base = 16 (low word had 16 zero bits)
       JMP  FFS32_LOOP
FFS32_LO:
       CLR  R0                 ; base = 0
FFS32_LOOP:
       INC  R0                 ; count (1-based)
       SRL  R2,1               ; shift LSB into carry
       JNC  FFS32_LOOP         ; no carry = bit was 0, keep looking
       B    *R11
FFS32_ZERO:
       CLR  R0
       B    *R11

;----------------------------------------------------------------------
; __ffsdi2 - Find first set in 64-bit value
;----------------------------------------------------------------------
__ffsdi2:
; Check from lowest word to highest
       MOV  R3,R4              ; word3 (lowest)
       JNE  FFS64_W3
       MOV  R2,R4              ; word2
       JNE  FFS64_W2
       MOV  R1,R4              ; word1
       JNE  FFS64_W1
       MOV  R0,R4              ; word0 (highest)
       JNE  FFS64_W0
; All zero
       CLR  R0
       B    *R11

FFS64_W3:
       CLR  R0                 ; base = 0
       JMP  FFS64_LOOP
FFS64_W2:
       LI   R0,16
       JMP  FFS64_LOOP
FFS64_W1:
       LI   R0,32
       JMP  FFS64_LOOP
FFS64_W0:
       LI   R0,48

FFS64_LOOP:
       INC  R0                 ; 1-based counting
       SRL  R4,1
       JNC  FFS64_LOOP
       B    *R11
