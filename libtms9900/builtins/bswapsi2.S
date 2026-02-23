; libtms9900 - __bswapsi2: Byte-swap a 32-bit value
;
; Hand-optimized TMS9900 assembly replacing compiler-rt C implementation.
;
; Input:  R0:R1 = value [A,B,C,D] (high:low, big-endian)
; Output: R0:R1 = swapped [D,C,B,A]
;
; Algorithm:
;   SWPB R0: [A,B] -> [B,A]
;   SWPB R1: [C,D] -> [D,C]
;   Exchange R0 and R1: R0=[D,C], R1=[B,A]
;
; 6 instructions, ~40 cycles (C version: ~30+ instructions)

       .text
       .global __bswapsi2

__bswapsi2:
       SWPB R0                ; [A,B] -> [B,A]
       SWPB R1                ; [C,D] -> [D,C]
       MOV  R0,R2             ; R2 = [B,A]
       MOV  R1,R0             ; R0 = [D,C]
       MOV  R2,R1             ; R1 = [B,A]
       B    *R11
