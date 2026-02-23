; libtms9900 - __ctzsi2: Count trailing zeros of a 32-bit value
;
; Hand-optimized TMS9900 assembly replacing compiler-rt C implementation.
;
; Input:  R0:R1 = 32-bit value (high:low)
; Output: R0 = number of trailing zero bits (0-32)
;
; Algorithm:
;   If low word nonzero, count trailing zeros in low word (0-15)
;   If low word zero, count = 16 + trailing zeros in high word
;   If both zero, return 32
;
; Uses SRL to shift LSB into carry flag, loop until a 1-bit is found.
; Average ~8 iterations for random inputs, worst case 32 for input=0.
;
; ~14 instructions total (C version: ~40+ instructions)

       .text
       .global __ctzsi2

__ctzsi2:
       MOV  R1,R2             ; R2 = low word, sets flags
       JNE  CTZ_LO            ; low word nonzero -> count it
       ; Low word is zero
       MOV  R0,R2             ; R2 = high word, sets flags
       JEQ  CTZ_32            ; both words zero -> return 32
       LI   R0,16             ; start count at 16
       JMP  CTZ_LOOP
CTZ_LO:
       CLR  R0                ; start count at 0
CTZ_LOOP:
       SRL  R2,1              ; shift LSB into carry
       JOC  CTZ_DONE          ; carry set = found a 1-bit
       INC  R0                ; count++
       JMP  CTZ_LOOP
CTZ_32:
       LI   R0,32
CTZ_DONE:
       B    *R11
