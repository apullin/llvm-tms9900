; libtms9900 - __clzdi2: Count leading zeros of a 64-bit value
;
; Hand-optimized TMS9900 assembly replacing compiler-rt C implementation.
;
; Input:  R0:R1:R2:R3 = 64-bit value (high to low words)
; Output: R0 = number of leading zero bits (0-64)
;
; Algorithm:
;   Find the first nonzero word from high to low.
;   Count leading zeros within that word using SLA loop.
;   Add 16 * (number of zero words above).
;
; Precondition: a != 0 (per compiler-rt spec), but we handle 0 anyway.

       .text
       .global __clzdi2

__clzdi2:
; Check word0 (highest)
       MOV  R0,R4
       JNE  CLZDI_W0
; Check word1
       MOV  R1,R4
       JNE  CLZDI_W1
; Check word2
       MOV  R2,R4
       JNE  CLZDI_W2
; Check word3 (lowest)
       MOV  R3,R4
       JNE  CLZDI_W3
; All zero -> return 64
       LI   R0,64
       B    *R11

CLZDI_W0:
       CLR  R0                 ; base count = 0
       JMP  CLZDI_LOOP
CLZDI_W1:
       LI   R0,16              ; base count = 16
       JMP  CLZDI_LOOP
CLZDI_W2:
       LI   R0,32              ; base count = 32
       JMP  CLZDI_LOOP
CLZDI_W3:
       LI   R0,48              ; base count = 48

CLZDI_LOOP:
       SLA  R4,1               ; shift MSB into carry
       JOC  CLZDI_DONE         ; carry set = found a 1-bit
       INC  R0
       JMP  CLZDI_LOOP
CLZDI_DONE:
       B    *R11
