; libtms9900 - Checked absolute value and negate operations (32-bit)
;
; __absvsi2: Absolute value of 32-bit signed int, abort on overflow
; __negvsi2: Negate 32-bit signed int, abort on overflow
;
; Input:  R0:R1 = 32-bit signed value (high:low)
; Output: R0:R1 = |a| or -a
; Effect: calls abort if a == INT32_MIN (0x80000000)
;
; INT32_MIN = 0x80000000 has no positive representation in 32 bits.

       .text
       .global __absvsi2
       .global __negvsi2

;----------------------------------------------------------------------
; __absvsi2 - Absolute value with overflow check
;----------------------------------------------------------------------
__absvsi2:
; Check for INT32_MIN
       CI   R0,0x8000
       JNE  ABSV_OK
       MOV  R1,R1
       JEQ  ABSV_ABORT         ; R0:R1 == 0x8000:0x0000 = INT32_MIN
ABSV_OK:
; If negative, negate
       MOV  R0,R0              ; test sign bit
       JGT  ABSV_DONE          ; positive -> return as-is
       JEQ  ABSV_CHK           ; zero high word, check low
       JMP  ABSV_NEG           ; negative -> negate
ABSV_CHK:
; R0 == 0, low word doesn't matter for sign (it's positive)
ABSV_DONE:
       B    *R11
ABSV_NEG:
       INV  R1
       INV  R0
       INC  R1
       JNC  ABSV_DONE2
       INC  R0
ABSV_DONE2:
       B    *R11
ABSV_ABORT:
       B    @abort

;----------------------------------------------------------------------
; __negvsi2 - Negate with overflow check
; Same check: abort if a == INT32_MIN
;----------------------------------------------------------------------
__negvsi2:
; Check for INT32_MIN
       CI   R0,0x8000
       JNE  NEGV_OK
       MOV  R1,R1
       JEQ  NEGV_ABORT         ; INT32_MIN -> abort
NEGV_OK:
       INV  R1
       INV  R0
       INC  R1
       JNC  NEGV_DONE
       INC  R0
NEGV_DONE:
       B    *R11
NEGV_ABORT:
       B    @abort
