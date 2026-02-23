; libtms9900 - __clzsi2: Count leading zeros of a 32-bit value
;
; Hand-optimized TMS9900 assembly replacing compiler-rt C implementation.
;
; Input:  R0:R1 = 32-bit value (high:low)
; Output: R0 = number of leading zero bits (0-32)
;
; Algorithm:
;   If high word nonzero, count leading zeros in high word (0-15)
;   If high word zero, count = 16 + leading zeros in low word
;   If both zero, return 32
;
; Uses SLA to shift MSB into carry flag, loop until a 1-bit is found.
; Average ~8 iterations for random inputs, worst case 32 for input=0.
;
; ~14 instructions total (C binary search version: ~50+ instructions)

       .text
       .global __clzsi2

__clzsi2:
       MOV  R0,R2             ; R2 = high word, sets flags
       JNE  CLZ_HI            ; high word nonzero -> count it
       ; High word is zero
       MOV  R1,R2             ; R2 = low word, sets flags
       JEQ  CLZ_32            ; both words zero -> return 32
       LI   R0,16             ; start count at 16
       JMP  CLZ_LOOP
CLZ_HI:
       CLR  R0                ; start count at 0
CLZ_LOOP:
       SLA  R2,1              ; shift MSB into carry
       JOC  CLZ_DONE          ; carry set = found a 1-bit
       INC  R0                ; count++
       JMP  CLZ_LOOP
CLZ_32:
       LI   R0,32
CLZ_DONE:
       B    *R11
