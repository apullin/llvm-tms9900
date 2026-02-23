; libtms9900 - Checked absolute value and negate operations (64-bit)
;
; __absvdi2: Absolute value of 64-bit signed int, abort on overflow
; __negvdi2: Negate 64-bit signed int, abort on overflow
;
; Input:  R0:R1:R2:R3 = 64-bit signed value (high to low words)
; Output: R0:R1:R2:R3 = |a| or -a
; Effect: calls abort if a == INT64_MIN (0x8000000000000000)
;
; INT64_MIN has no positive representation in 64 bits.

       .text
       .global __absvdi2
       .global __negvdi2

;----------------------------------------------------------------------
; __absvdi2 - 64-bit absolute value with overflow check
;----------------------------------------------------------------------
__absvdi2:
; Check for INT64_MIN: R0=0x8000, R1=R2=R3=0
       CI   R0,0x8000
       JNE  ABSVDI_OK
       MOV  R1,R1
       JNE  ABSVDI_OK
       MOV  R2,R2
       JNE  ABSVDI_OK
       MOV  R3,R3
       JEQ  ABSVDI_ABORT       ; all conditions met -> INT64_MIN
ABSVDI_OK:
; If non-negative, return as-is
       MOV  R0,R0
       JGT  ABSVDI_RET         ; positive high word
       JEQ  ABSVDI_RET         ; zero high word (value is non-negative)
; Negative -> negate (two's complement)
       INV  R0
       INV  R1
       INV  R2
       INV  R3
       INC  R3
       JNE  ABSVDI_RET
       INC  R2
       JNE  ABSVDI_RET
       INC  R1
       JNE  ABSVDI_RET
       INC  R0
ABSVDI_RET:
       B    *R11
ABSVDI_ABORT:
       B    @abort

;----------------------------------------------------------------------
; __negvdi2 - 64-bit negate with overflow check
;----------------------------------------------------------------------
__negvdi2:
; Check for INT64_MIN
       CI   R0,0x8000
       JNE  NEGVDI_OK
       MOV  R1,R1
       JNE  NEGVDI_OK
       MOV  R2,R2
       JNE  NEGVDI_OK
       MOV  R3,R3
       JEQ  NEGVDI_ABORT
NEGVDI_OK:
       INV  R0
       INV  R1
       INV  R2
       INV  R3
       INC  R3
       JNE  NEGVDI_RET
       INC  R2
       JNE  NEGVDI_RET
       INC  R1
       JNE  NEGVDI_RET
       INC  R0
NEGVDI_RET:
       B    *R11
NEGVDI_ABORT:
       B    @abort
