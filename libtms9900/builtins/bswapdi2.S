; libtms9900 - __bswapdi2: Byte-swap a 64-bit value
;
; Hand-optimized TMS9900 assembly replacing compiler-rt C implementation.
;
; Input:  R0:R1:R2:R3 = 64-bit value [A,B,C,D,E,F,G,H] (bytes high to low)
; Output: R0:R1:R2:R3 = swapped [H,G,F,E,D,C,B,A]
;
; Algorithm:
;   SWPB each word: swap bytes within words
;   Then reverse word order: R0<->R3, R1<->R2
;
; 10 instructions, ~60 cycles (C version: ~510 instructions)

       .text
       .global __bswapdi2

__bswapdi2:
; Swap bytes within each word
       SWPB R0                ; [A,B] -> [B,A]
       SWPB R1                ; [C,D] -> [D,C]
       SWPB R2                ; [E,F] -> [F,E]
       SWPB R3                ; [G,H] -> [H,G]
; Reverse word order: R0<->R3, R1<->R2
       MOV  R0,R4             ; temp = R0 [B,A]
       MOV  R3,R0             ; R0 = R3 [H,G]
       MOV  R4,R3             ; R3 = temp [B,A]
       MOV  R1,R4             ; temp = R1 [D,C]
       MOV  R2,R1             ; R1 = R2 [F,E]
       MOV  R4,R2             ; R2 = temp [D,C]
       B    *R11
