; libtms9900 - __negdi2: Negate a 64-bit value
;
; Hand-optimized TMS9900 assembly replacing compiler-rt C implementation.
;
; Input:  R0:R1:R2:R3 = 64-bit value (high to low words)
; Output: R0:R1:R2:R3 = negated value (-a)
;
; Algorithm: Two's complement = bitwise NOT + 1
;   INV all 4 words, then increment the 64-bit value by 1.
;   Carry propagation: if INC wraps to 0, propagate to next word.
;
; 12 instructions worst case (C version: ~110 instructions at O2)

       .text
       .global __negdi2

__negdi2:
       INV  R0
       INV  R1
       INV  R2
       INV  R3
; Add 1 to lowest word, propagate carry upward
       INC  R3                 ; R3 += 1
       JNE  NEGDI_DONE         ; if R3 != 0, no carry to propagate
       INC  R2
       JNE  NEGDI_DONE
       INC  R1
       JNE  NEGDI_DONE
       INC  R0
NEGDI_DONE:
       B    *R11
