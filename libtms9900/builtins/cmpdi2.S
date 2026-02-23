; libtms9900 - 64-bit comparison operations
;
; __cmpdi2:  Signed 64-bit compare
; __ucmpdi2: Unsigned 64-bit compare
;
; Input:  R0:R1:R2:R3 = first 64-bit value (a)
;         Stack: second 64-bit value (b)
;           *R10     = b word0 (highest)
;           @2(R10)  = b word1
;           @4(R10)  = b word2
;           @6(R10)  = b word3 (lowest)
; Output: R0:R1 = 0 if a < b, 1 if a == b, 2 if a > b
;         (si_int = int32_t return, so 32-bit in R0:R1)
;
; For __cmpdi2, the highest word comparison is signed;
; remaining words are unsigned (since high words are equal).
; For __ucmpdi2, all comparisons are unsigned.
;
; TMS9900 C instruction: C Rx,Ry sets flags based on Rx - Ry.
; JGT = Rx > Ry (signed), JLT = Rx < Ry (signed)
; JH = Rx > Ry (unsigned), JL = Rx < Ry (unsigned)

       .text
       .global __cmpdi2
       .global __ucmpdi2

;----------------------------------------------------------------------
; __cmpdi2 - Signed 64-bit compare
;----------------------------------------------------------------------
__cmpdi2:
; Compare word0 (highest) — SIGNED comparison
       C    R0,*R10            ; compare a.word0 vs b.word0
       JGT  CMP_GT             ; a > b (signed)
       JLT  CMP_LT             ; a < b (signed)
; word0 equal, compare word1 — UNSIGNED
       C    R1,@2(R10)
       JH   CMP_GT             ; a > b (unsigned)
       JL   CMP_LT             ; a < b (unsigned)
; word1 equal, compare word2 — UNSIGNED
       C    R2,@4(R10)
       JH   CMP_GT
       JL   CMP_LT
; word2 equal, compare word3 — UNSIGNED
       C    R3,@6(R10)
       JH   CMP_GT
       JL   CMP_LT
; All equal: return 1 (R0:R1 = 0x00000001)
       CLR  R0
       LI   R1,1
       B    *R11
CMP_LT:
; Return 0 (R0:R1 = 0x00000000)
       CLR  R0
       CLR  R1
       B    *R11
CMP_GT:
; Return 2 (R0:R1 = 0x00000002)
       CLR  R0
       LI   R1,2
       B    *R11

;----------------------------------------------------------------------
; __ucmpdi2 - Unsigned 64-bit compare
;----------------------------------------------------------------------
__ucmpdi2:
; Compare word0 (highest) — UNSIGNED comparison
       C    R0,*R10
       JH   UCMP_GT
       JL   UCMP_LT
; word0 equal, compare word1 — UNSIGNED
       C    R1,@2(R10)
       JH   UCMP_GT
       JL   UCMP_LT
; word1 equal, compare word2 — UNSIGNED
       C    R2,@4(R10)
       JH   UCMP_GT
       JL   UCMP_LT
; word2 equal, compare word3 — UNSIGNED
       C    R3,@6(R10)
       JH   UCMP_GT
       JL   UCMP_LT
; All equal: return 1
       CLR  R0
       LI   R1,1
       B    *R11
UCMP_LT:
       CLR  R0
       CLR  R1
       B    *R11
UCMP_GT:
       CLR  R0
       LI   R1,2
       B    *R11
