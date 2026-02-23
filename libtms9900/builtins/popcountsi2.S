; libtms9900 - Population count (popcount) operations
;
; __popcountsi2: Count set bits in 32-bit value
; __popcountdi2: Count set bits in 64-bit value
;
; __popcountsi2:
;   Input:  R0:R1 = 32-bit value (high:low)
;   Output: R0 = number of 1-bits (0-32)
;
; __popcountdi2:
;   Input:  R0:R1:R2:R3 = 64-bit value
;   Output: R0 = number of 1-bits (0-64)
;
; Algorithm: Shift bits out one at a time using SLA, count carries.
;   16 iterations per word (constant time per word).
;   Total: 32 iterations for si, 64 for di.

       .text
       .global __popcountsi2
       .global __popcountdi2

;----------------------------------------------------------------------
; __popcountdi2 - 64-bit popcount
;----------------------------------------------------------------------
__popcountdi2:
       CLR  R4                 ; total count = 0
; Count word0 (R0)
       LI   R5,16
PCDI_W0:
       SLA  R0,1
       JNC  PCDI_W0N
       INC  R4
PCDI_W0N:
       DEC  R5
       JNE  PCDI_W0
; Count word1 (R1)
       LI   R5,16
PCDI_W1:
       SLA  R1,1
       JNC  PCDI_W1N
       INC  R4
PCDI_W1N:
       DEC  R5
       JNE  PCDI_W1
; Count word2 (R2)
       LI   R5,16
PCDI_W2:
       SLA  R2,1
       JNC  PCDI_W2N
       INC  R4
PCDI_W2N:
       DEC  R5
       JNE  PCDI_W2
; Count word3 (R3)
       LI   R5,16
PCDI_W3:
       SLA  R3,1
       JNC  PCDI_W3N
       INC  R4
PCDI_W3N:
       DEC  R5
       JNE  PCDI_W3
; Return total
       MOV  R4,R0
       B    *R11

;----------------------------------------------------------------------
; __popcountsi2 - 32-bit popcount
;----------------------------------------------------------------------
__popcountsi2:
       CLR  R2                 ; total count = 0
; Count word0 (R0)
       LI   R3,16
PCSI_W0:
       SLA  R0,1
       JNC  PCSI_W0N
       INC  R2
PCSI_W0N:
       DEC  R3
       JNE  PCSI_W0
; Count word1 (R1)
       LI   R3,16
PCSI_W1:
       SLA  R1,1
       JNC  PCSI_W1N
       INC  R2
PCSI_W1N:
       DEC  R3
       JNE  PCSI_W1
; Return total
       MOV  R2,R0
       B    *R11
