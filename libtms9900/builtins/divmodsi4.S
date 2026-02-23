; libtms9900 - Combined division/modulo operations (32-bit)
;
; __divmodsi4:  Signed 32-bit divmod: returns a/b, *rem = a%b
; __udivmodsi4: Unsigned 32-bit divmod: returns a/b, *rem = a%b
;
; Input:  R0:R1 = a (32-bit), R2:R3 = b (32-bit)
;         *R10 = pointer to remainder output (32-bit)
; Output: R0:R1 = quotient
;         *rem = remainder
;
; Uses UDIV32 internal routine from div32.S:
;   Input:  R0:R1 = dividend, R2:R3 = divisor
;   Output: R4:R5 = quotient, R6:R7 = remainder
;   Clobbers: R0, R1, R8

       .text
       .global __divmodsi4
       .global __udivmodsi4

;----------------------------------------------------------------------
; __udivmodsi4 - Unsigned 32-bit divmod
;----------------------------------------------------------------------
__udivmodsi4:
       DECT R10
       MOV  R11,*R10
; Save rem pointer (was at old SP+0, now at SP+2 after push)
       MOV  @2(R10),R8         ; R8 = rem pointer -- BUT UDIV32 clobbers R8!
       DECT R10
       MOV  R8,*R10            ; push rem pointer to save it
       BL   @UDIV32            ; R4:R5 = quotient, R6:R7 = remainder
       MOV  *R10+,R8           ; pop rem pointer
; Store remainder
       MOV  R6,*R8             ; rem_high
       MOV  R7,@2(R8)          ; rem_low
; Return quotient
       MOV  R4,R0
       MOV  R5,R1
       MOV  *R10+,R11
       B    *R11

;----------------------------------------------------------------------
; __divmodsi4 - Signed 32-bit divmod
;
; Algorithm:
;   Save signs, take absolute values, unsigned divide,
;   negate quotient if signs differ, negate remainder if dividend was negative.
;----------------------------------------------------------------------
__divmodsi4:
       DECT R10
       MOV  R11,*R10
       DECT R10
       MOV  R9,*R10

; Save rem pointer (was at old SP+0, now at SP+4 after 2 pushes)
       MOV  @4(R10),R9         ; R9 = rem pointer -- will be saved on stack too
       DECT R10
       MOV  R9,*R10            ; push rem pointer

       CLR  R9                 ; R9 = sign flags

; Handle dividend sign
       MOV  R0,R0
       JGT  DMS_D1
       JEQ  DMS_D1
; Dividend is negative
       ORI  R9,3               ; R9 bit 0 = quotient sign flip, bit 1 = rem sign flip
       INV  R1
       INV  R0
       INC  R1
       JNC  DMS_D1
       INC  R0
DMS_D1:
; Handle divisor sign
       MOV  R2,R2
       JGT  DMS_D2
       JEQ  DMS_D2
; Divisor is negative
       LI   R4,1
       XOR  R4,R9              ; flip bit 0 (quotient sign)
       INV  R3
       INV  R2
       INC  R3
       JNC  DMS_D2
       INC  R2
DMS_D2:
; R0:R1 = |a|, R2:R3 = |b|
       DECT R10
       MOV  R9,*R10            ; push sign flags (UDIV32 clobbers R8)
       BL   @UDIV32            ; R4:R5 = quotient, R6:R7 = remainder
       MOV  *R10+,R9           ; pop sign flags

; Apply quotient sign (bit 0 of R9)
       MOV  R9,R0
       ANDI R0,1
       JEQ  DMS_QPOS
; Negate quotient
       INV  R5
       INV  R4
       INC  R5
       JNC  DMS_QPOS
       INC  R4
DMS_QPOS:
; Apply remainder sign (bit 1 of R9)
       MOV  R9,R0
       ANDI R0,2
       JEQ  DMS_RPOS
; Negate remainder
       INV  R7
       INV  R6
       INC  R7
       JNC  DMS_RPOS
       INC  R6
DMS_RPOS:
; Store remainder
       MOV  *R10+,R8           ; pop rem pointer into R8
       MOV  R6,*R8
       MOV  R7,@2(R8)
; Return quotient
       MOV  R4,R0
       MOV  R5,R1

       MOV  *R10+,R9
       MOV  *R10+,R11
       B    *R11
