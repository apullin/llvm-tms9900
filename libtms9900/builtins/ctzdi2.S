; libtms9900 - __ctzdi2: Count trailing zeros of a 64-bit value
;
; Hand-optimized TMS9900 assembly replacing compiler-rt C implementation.
;
; Input:  R0:R1:R2:R3 = 64-bit value (high to low words)
; Output: R0 = number of trailing zero bits (0-64)
;
; Algorithm:
;   Find the first nonzero word from low to high.
;   Count trailing zeros within that word using SRL loop.
;   Add 16 * (number of zero words below).
;
; Precondition: a != 0 (per compiler-rt spec), but we handle 0 anyway.

       .text
       .global __ctzdi2

__ctzdi2:
; Check word3 (lowest)
       MOV  R3,R4
       JNE  CTZDI_W3
; Check word2
       MOV  R2,R4
       JNE  CTZDI_W2
; Check word1
       MOV  R1,R4
       JNE  CTZDI_W1
; Check word0 (highest)
       MOV  R0,R4
       JNE  CTZDI_W0
; All zero -> return 64
       LI   R0,64
       B    *R11

CTZDI_W3:
       CLR  R0                 ; base count = 0
       JMP  CTZDI_LOOP
CTZDI_W2:
       LI   R0,16              ; base count = 16
       JMP  CTZDI_LOOP
CTZDI_W1:
       LI   R0,32              ; base count = 32
       JMP  CTZDI_LOOP
CTZDI_W0:
       LI   R0,48              ; base count = 48

CTZDI_LOOP:
       SRL  R4,1               ; shift LSB into carry
       JOC  CTZDI_DONE         ; carry set = found a 1-bit
       INC  R0
       JMP  CTZDI_LOOP
CTZDI_DONE:
       B    *R11
