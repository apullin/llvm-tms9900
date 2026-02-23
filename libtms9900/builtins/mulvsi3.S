; libtms9900 - Checked 32-bit multiply
;
; __mulvsi3: Signed 32-bit multiply, abort on overflow
;
; Input:  R0:R1 = a (32-bit signed), R2:R3 = b (32-bit signed)
; Output: R0:R1 = a * b (lower 32 bits)
; Effect: calls abort on signed overflow
;
; Algorithm:
;   Handle MIN * {0,1} and {0,1} * MIN specially.
;   Take absolute values, check |a| * |b| overflow via
;   comparison with INT32_MAX (or INT32_MIN for different signs).
;   Then compute and return a * b.
;
; INT32_MIN = 0x80000000, INT32_MAX = 0x7FFFFFFF
;
; For efficiency, we use a simpler approach:
;   1. Compute the full product using unsigned multiply
;   2. Negate if signs differ
;   3. Check that the result fits in 32-bit signed
;
; Actually, the simplest correct approach for TMS9900:
;   Compute the 64-bit product of |a| * |b|.
;   If the high 32 bits are non-zero, overflow.
;   If same sign, check result fits in [0, 0x7FFFFFFF].
;   If diff sign, check result fits in [0, 0x80000000].
;   Then negate if signs differ.

       .text
       .global __mulvsi3

__mulvsi3:
       DECT R10
       MOV  R11,*R10
       DECT R10
       MOV  R9,*R10

; Determine result sign and take absolute values
       CLR  R9                 ; R9 = 0 means positive result
; Check/negate a
       MOV  R0,R0
       JEQ  MULV_ACHK0
       JGT  MULV_APOS
; a is negative
       INV  R9                 ; flip result sign
       INV  R1
       INV  R0
       INC  R1
       JNC  MULV_APOS
       INC  R0
       JMP  MULV_APOS
MULV_ACHK0:
; R0==0: check if a is zero
       MOV  R1,R1
       JEQ  MULV_ZERO          ; a == 0, result = 0
MULV_APOS:
; Check/negate b
       MOV  R2,R2
       JEQ  MULV_BCHK0
       JGT  MULV_BPOS
; b is negative
       INV  R9                 ; flip result sign
       INV  R3
       INV  R2
       INC  R3
       JNC  MULV_BPOS
       INC  R2
       JMP  MULV_BPOS
MULV_BCHK0:
       MOV  R3,R3
       JEQ  MULV_ZERO          ; b == 0, result = 0
MULV_BPOS:
; Now R0:R1 = |a|, R2:R3 = |b|, R9 = sign (0 or -1)
; Compute |a| * |b| keeping upper 32 bits for overflow check.
; Product of 32-bit values can be up to 64 bits.
; |a| = Ah*2^16 + Al, |b| = Bh*2^16 + Bl
; Product = Ah*Bh*2^32 + (Ah*Bl + Al*Bh)*2^16 + Al*Bl
;
; We need to detect if result > 0x7FFFFFFF (same sign) or > 0x80000000 (diff sign).
; Strategy: compute the 32-bit multiply (__mulsi3 logic), but also track overflow.
;
; For simplicity: compute the 32-bit result and check the upper 32 bits.
; Overflow if any contribution to the upper 32 bits exists.

       DECT R10
       MOV  R8,*R10            ; save R8
       DECT R10
       MOV  R7,*R10            ; save R7

       MOV  R0,R4              ; R4 = Ah
       MOV  R1,R5              ; R5 = Al
       CLR  R7                 ; R7 = overflow word (upper 32-bit accumulator)

; Step 1: Ah * Bh — if nonzero, overflow (contributes to bits 32-63)
       MOV  R4,R4
       JEQ  MULV_S2
       MOV  R2,R2
       JEQ  MULV_S2
; Ah * Bh is nonzero → result needs > 32 bits → overflow
       JMP  MULV_OVERFLOW

MULV_S2:
; Step 2: Al * Bl → full 32-bit result
       MOV  R5,R0              ; R0 = Al
       MPY  R3,R0              ; R0:R1 = Al * Bl (32-bit result)
       MOV  R0,R6              ; R6 = high(Al*Bl)
       MOV  R1,R8              ; R8 = low(Al*Bl)

; Step 3: Ah * Bl — only low word matters, high word goes to overflow
       MOV  R4,R4
       JEQ  MULV_S4            ; Ah == 0, skip
       MOV  R4,R0
       MPY  R3,R0              ; R0:R1 = Ah * Bl
       A    R0,R7              ; overflow accumulator += high(Ah*Bl)
       A    R1,R6              ; result_hi += low(Ah*Bl)
       JNC  MULV_S4
       INC  R7                 ; carry to overflow

MULV_S4:
; Step 4: Al * Bh — only low word matters, high word goes to overflow
       MOV  R2,R2
       JEQ  MULV_S5            ; Bh == 0, skip
       MOV  R5,R0
       MPY  R2,R0              ; R0:R1 = Al * Bh
       A    R0,R7              ; overflow += high
       A    R1,R6              ; result_hi += low
       JNC  MULV_S5
       INC  R7                 ; carry to overflow

MULV_S5:
; Now: R6:R8 = lower 32 bits of product, R7 = overflow indicator
; Check overflow: if R7 != 0, definitely overflow
       MOV  R7,R7
       JNE  MULV_OVERFLOW

; Check if result fits in signed range
; For same-sign result (R9==0): must be <= 0x7FFFFFFF (bit 15 of R6 must be 0)
; For diff-sign result (R9!=0): must be <= 0x80000000
       MOV  R9,R9
       JNE  MULV_DIFFSIGN
; Same sign: check R6 bit 15 (must be 0 for positive result)
       MOV  R6,R6
       JLT  MULV_OVERFLOW      ; bit 15 set -> result > 0x7FFFFFFF -> overflow
       JMP  MULV_RESULT
MULV_DIFFSIGN:
; Different sign: result will be negated. Max magnitude is 0x80000000.
       CI   R6,0x8000
       JH   MULV_OVERFLOW      ; R6 > 0x8000 -> overflow
       JL   MULV_RESULT        ; R6 < 0x8000 -> OK
; R6 == 0x8000: overflow only if R8 != 0 (magnitude > 0x80000000)
       MOV  R8,R8
       JNE  MULV_OVERFLOW

MULV_RESULT:
; Assemble result
       MOV  R6,R0
       MOV  R8,R1
; Apply sign
       MOV  R9,R9
       JEQ  MULV_DONE
       INV  R1
       INV  R0
       INC  R1
       JNC  MULV_DONE
       INC  R0
MULV_DONE:
       MOV  *R10+,R7
       MOV  *R10+,R8
       MOV  *R10+,R9
       MOV  *R10+,R11
       B    *R11

MULV_ZERO:
       CLR  R0
       CLR  R1
       MOV  *R10+,R9
       MOV  *R10+,R11
       B    *R11

MULV_OVERFLOW:
       B    @abort
