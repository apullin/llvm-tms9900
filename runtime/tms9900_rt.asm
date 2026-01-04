; TMS9900 Runtime Library for LLVM
;
; Calling convention for 32-bit operations:
;   Input:  First 32-bit arg in R0:R1 (high:low)
;           Second 32-bit arg in R2:R3 (high:low)
;   Output: 32-bit result in R0:R1 (high:low)
;   Clobbered: R0-R3, and function-specific temps
;
; All functions preserve R10 (SP), R11 (LR), R13-R15 (callee-saved)

       DEF  __mulsi3
       DEF  __divsi3
       DEF  __udivsi3
       DEF  __modsi3
       DEF  __umodsi3
       DEF  __ashlsi3
       DEF  __lshrsi3
       DEF  __ashrsi3

;======================================================================
; __mulsi3 - Signed/Unsigned 32-bit multiply
; Input:  R0:R1 = multiplicand (high:low)
;         R2:R3 = multiplier (high:low)
; Output: R0:R1 = product (high:low), lower 32 bits only
;
; Algorithm: (AH*2^16 + AL) * (BH*2^16 + BL)
;          = AH*BH*2^32 + (AH*BL + AL*BH)*2^16 + AL*BL
;          We only keep lower 32 bits, so ignore AH*BH term
;======================================================================
__mulsi3:
       MOV  R11,@MSAV11         ; Save return address
       MOV  R0,@M_AHI           ; Save A high
       MOV  R1,@M_ALO           ; Save A low
       MOV  R2,@M_BHI           ; Save B high
       MOV  R3,@M_BLO           ; Save B low

       ; AL * BL -> full 32-bit result
       MOV  R1,R0               ; R0 = AL
       MPY  R3,R0               ; R0:R1 = AL * BL
       MOV  R0,@M_RESHI         ; Save high word of AL*BL
       MOV  R1,@M_RESLO         ; Save low word of AL*BL

       ; AH * BL -> add to high word
       MOV  @M_AHI,R0           ; R0 = AH
       MOV  @M_BLO,R2           ; R2 = BL
       MPY  R2,R0               ; R0:R1 = AH * BL
       A    R1,@M_RESHI         ; Add low word to result high

       ; AL * BH -> add to high word
       MOV  @M_ALO,R0           ; R0 = AL
       MOV  @M_BHI,R2           ; R2 = BH
       MPY  R2,R0               ; R0:R1 = AL * BH
       A    R1,@M_RESHI         ; Add low word to result high

       ; Return result
       MOV  @M_RESHI,R0         ; R0 = result high
       MOV  @M_RESLO,R1         ; R1 = result low
       MOV  @MSAV11,R11         ; Restore return address
       B    *R11

; Multiply scratch space
M_AHI  DATA 0
M_ALO  DATA 0
M_BHI  DATA 0
M_BLO  DATA 0
M_RESHI DATA 0
M_RESLO DATA 0
MSAV11 DATA 0

;======================================================================
; __udivsi3 - Unsigned 32-bit divide
; Input:  R0:R1 = dividend (high:low)
;         R2:R3 = divisor (high:low)
; Output: R0:R1 = quotient (high:low)
;
; Shift-and-subtract (restoring division) algorithm:
; - We treat dividend:0 as a 64-bit number
; - Shift left, compare upper 32 bits with divisor
; - If >= divisor, subtract and set quotient bit
;======================================================================
__udivsi3:
       MOV  R11,@DSAV11         ; Save return address
       MOV  R13,@DSAV13         ; Save callee-saved
       MOV  R14,@DSAV14
       MOV  R15,@DSAV15

       ; Check for divide by zero
       MOV  R2,R4
       SOC  R3,R4               ; R4 = R2 | R3
       JNE  UDIV_OK
       ; Divide by zero - return all 1s
       SETO R0
       SETO R1
       JMP  UDIV_RET

UDIV_OK:
       ; R0:R1 = dividend (gets consumed as we shift bits out)
       ; R2:R3 = divisor (preserved)
       ; R4:R5 = remainder (starts at 0)
       ; R6:R7 = quotient (built up during loop)
       CLR  R4                  ; R4 = remainder high
       CLR  R5                  ; R5 = remainder low
       CLR  R6                  ; R6 = quotient high
       CLR  R7                  ; R7 = quotient low

       ; 32 iterations
       LI   R13,32

UDIV_LOOP:
       ; Shift quotient left (R6:R7)
       SLA  R7,1
       JNC  UDIV_QNC
       INC  R6
UDIV_QNC:
       SLA  R6,1

       ; Shift dividend left, MSB into remainder
       ; First, save MSB of R0 before shifting
       CLR  R14                 ; Carry for remainder
       MOV  R0,R0               ; Set status from R0
       JLT  UDIV_SETC           ; If negative (MSB set), set carry
       JMP  UDIV_DSHIFT
UDIV_SETC:
       LI   R14,1               ; Carry bit = MSB of dividend

UDIV_DSHIFT:
       ; Shift dividend R0:R1 left as 32-bit
       ; First shift R0 (losing MSB, which we saved in R14)
       SLA  R0,1
       ; Then shift R1, carrying its MSB into R0's LSB
       SLA  R1,1
       JNC  UDIV_DNC1
       ORI  R0,1                ; Carry from R1 MSB into R0 LSB
UDIV_DNC1:

       ; Shift remainder left and add dividend MSB from R14
       SLA  R5,1
       JNC  UDIV_RNC1
       INC  R4
UDIV_RNC1:
       SLA  R4,1
       A    R14,R5              ; Add old dividend MSB to remainder LSB
       JNC  UDIV_RNC2
       INC  R4
UDIV_RNC2:

       ; Compare remainder >= divisor
       C    R4,R2               ; Compare high
       JL   UDIV_NEXT
       JH   UDIV_SUB
       C    R5,R3               ; Compare low
       JL   UDIV_NEXT

UDIV_SUB:
       ; Subtract divisor from remainder
       S    R3,R5               ; Sub low
       JNC  UDIV_BORROW
       JMP  UDIV_SUBHI
UDIV_BORROW:
       DEC  R4
UDIV_SUBHI:
       S    R2,R4               ; Sub high

       ; Set quotient bit
       INC  R7
       JNC  UDIV_NEXT
       INC  R6

UDIV_NEXT:
       DEC  R13
       JNE  UDIV_LOOP

       ; Return quotient
       MOV  R6,R0
       MOV  R7,R1

UDIV_RET:
       MOV  @DSAV15,R15
       MOV  @DSAV14,R14
       MOV  @DSAV13,R13
       MOV  @DSAV11,R11
       B    *R11

; Divide scratch space
DSAV11 DATA 0
DSAV13 DATA 0
DSAV14 DATA 0
DSAV15 DATA 0

;======================================================================
; __divsi3 - Signed 32-bit divide
; Input:  R0:R1 = dividend (high:low)
;         R2:R3 = divisor (high:low)
; Output: R0:R1 = quotient (high:low)
;
; Handle signs, then use unsigned divide
;======================================================================
__divsi3:
       MOV  R11,@SDSAV11        ; Save return address
       CLR  @SD_SIGN            ; Clear sign flag

       ; Check if dividend is negative
       MOV  R0,R0               ; Test high word
       JGT  SDIV_POS1
       JEQ  SDIV_POS1           ; Zero is positive
       ; Negate dividend
       INV  R0
       INV  R1
       INC  R1
       JNC  SDIV_ND1
       INC  R0
SDIV_ND1:
       INC  @SD_SIGN            ; Toggle sign

SDIV_POS1:
       ; Check if divisor is negative
       MOV  R2,R2               ; Test high word
       JGT  SDIV_POS2
       JEQ  SDIV_POS2           ; Zero is positive
       ; Negate divisor
       INV  R2
       INV  R3
       INC  R3
       JNC  SDIV_ND2
       INC  R2
SDIV_ND2:
       INC  @SD_SIGN            ; Toggle sign

SDIV_POS2:
       ; Do unsigned divide
       BL   @__udivsi3

       ; Check if result should be negative
       MOV  @SD_SIGN,R4
       ANDI R4,1                ; Check low bit
       JEQ  SDIV_RET

       ; Negate result
       INV  R0
       INV  R1
       INC  R1
       JNC  SDIV_RET
       INC  R0

SDIV_RET:
       MOV  @SDSAV11,R11
       B    *R11

SD_SIGN DATA 0
SDSAV11 DATA 0

;======================================================================
; __umodsi3 - Unsigned 32-bit modulo
; Input:  R0:R1 = dividend (high:low)
;         R2:R3 = divisor (high:low)
; Output: R0:R1 = remainder (high:low)
;
; Same algorithm as udivsi3 but returns remainder
;======================================================================
__umodsi3:
       MOV  R11,@UMSAV11        ; Save return address
       MOV  R13,@UMSAV13        ; Save callee-saved
       MOV  R14,@UMSAV14
       MOV  R15,@UMSAV15

       ; Check for divide by zero
       MOV  R2,R4
       SOC  R3,R4
       JNE  UMOD_OK
       ; Mod by zero - return dividend
       JMP  UMOD_RET

UMOD_OK:
       ; R0:R1 = dividend (gets consumed)
       ; R2:R3 = divisor (preserved)
       ; R4:R5 = remainder (starts at 0, returned)
       CLR  R4
       CLR  R5

       ; 32 iterations
       LI   R13,32

UMOD_LOOP:
       ; Shift dividend left, MSB into remainder
       ; First, save MSB of R0 before shifting
       CLR  R14                 ; Carry for remainder
       MOV  R0,R0               ; Set status from R0
       JLT  UMOD_SETC           ; If negative (MSB set), set carry
       JMP  UMOD_DSHIFT
UMOD_SETC:
       LI   R14,1               ; Carry bit = MSB of dividend

UMOD_DSHIFT:
       ; Shift dividend R0:R1 left as 32-bit
       SLA  R0,1
       SLA  R1,1
       JNC  UMOD_DNC1
       ORI  R0,1                ; Carry from R1 MSB into R0 LSB
UMOD_DNC1:

       ; Shift remainder left and add old dividend MSB from R14
       SLA  R5,1
       JNC  UMOD_RNC1
       INC  R4
UMOD_RNC1:
       SLA  R4,1
       A    R14,R5
       JNC  UMOD_RNC2
       INC  R4
UMOD_RNC2:

       ; Compare remainder >= divisor
       C    R4,R2
       JL   UMOD_NEXT
       JH   UMOD_SUB
       C    R5,R3
       JL   UMOD_NEXT

UMOD_SUB:
       ; Subtract divisor from remainder
       S    R3,R5
       JNC  UMOD_BORROW
       JMP  UMOD_SUBHI
UMOD_BORROW:
       DEC  R4
UMOD_SUBHI:
       S    R2,R4

UMOD_NEXT:
       DEC  R13
       JNE  UMOD_LOOP

       ; Return remainder
       MOV  R4,R0
       MOV  R5,R1

UMOD_RET:
       MOV  @UMSAV15,R15
       MOV  @UMSAV14,R14
       MOV  @UMSAV13,R13
       MOV  @UMSAV11,R11
       B    *R11

UMSAV11 DATA 0
UMSAV13 DATA 0
UMSAV14 DATA 0
UMSAV15 DATA 0

;======================================================================
; __modsi3 - Signed 32-bit modulo
; Input:  R0:R1 = dividend (high:low)
;         R2:R3 = divisor (high:low)
; Output: R0:R1 = remainder (high:low)
;
; Remainder has same sign as dividend (C convention)
;======================================================================
__modsi3:
       MOV  R11,@SMSAV11
       CLR  @SM_SIGN            ; Clear sign flag

       ; Check if dividend is negative
       MOV  R0,R0
       JGT  SMOD_POS1
       JEQ  SMOD_POS1
       ; Negate dividend
       INV  R0
       INV  R1
       INC  R1
       JNC  SMOD_ND1
       INC  R0
SMOD_ND1:
       INC  @SM_SIGN            ; Remember dividend was negative

SMOD_POS1:
       ; Check if divisor is negative (need absolute value)
       MOV  R2,R2
       JGT  SMOD_POS2
       JEQ  SMOD_POS2
       ; Negate divisor
       INV  R2
       INV  R3
       INC  R3
       JNC  SMOD_POS2
       INC  R2

SMOD_POS2:
       ; Do unsigned modulo
       BL   @__umodsi3

       ; Check if result should be negative
       MOV  @SM_SIGN,R4
       JEQ  SMOD_RET

       ; Negate result
       INV  R0
       INV  R1
       INC  R1
       JNC  SMOD_RET
       INC  R0

SMOD_RET:
       MOV  @SMSAV11,R11
       B    *R11

SM_SIGN DATA 0
SMSAV11 DATA 0

;======================================================================
; __ashlsi3 - 32-bit left shift
; Input:  R0:R1 = value (high:low)
;         R2 = shift amount
; Output: R0:R1 = value << shift_amount
;
; Algorithm: Loop shifting 1 bit at a time
;======================================================================
__ashlsi3:
       MOV  R2,R2               ; Check shift amount
       JEQ  ASHL_RET            ; If 0, return unchanged

       ; Check for shift >= 32
       CI   R2,32
       JL   ASHL_OK
       CLR  R0                  ; Return 0
       CLR  R1
       B    *R11

ASHL_OK:
       ; Check for shift >= 16 (word shift optimization)
       CI   R2,16
       JL   ASHL_LOOP
       ; Shift by 16: move low word to high, clear low
       MOV  R1,R0
       CLR  R1
       AI   R2,-16              ; Remaining shift
       JEQ  ASHL_RET

ASHL_LOOP:
       ; Shift R0:R1 left by 1 bit
       ; Must shift R0 first, then use carry from R1 shift
       SLA  R0,1                ; Shift high word left (makes room in bit 0)
       SLA  R1,1                ; Shift low word left (sets carry from its MSB)
       JNC  ASHL_NC             ; If no carry (MSB was 0)
       INC  R0                  ; Set LSB of high word
ASHL_NC:
       DEC  R2
       JNE  ASHL_LOOP

ASHL_RET:
       B    *R11

;======================================================================
; __lshrsi3 - 32-bit logical right shift
; Input:  R0:R1 = value (high:low)
;         R2 = shift amount
; Output: R0:R1 = value >> shift_amount (zero fill)
;
; Algorithm: Loop shifting 1 bit at a time
;======================================================================
__lshrsi3:
       MOV  R2,R2               ; Check shift amount
       JEQ  LSHR_RET            ; If 0, return unchanged

       ; Check for shift >= 32
       CI   R2,32
       JL   LSHR_OK
       CLR  R0                  ; Return 0
       CLR  R1
       B    *R11

LSHR_OK:
       ; Check for shift >= 16 (word shift optimization)
       CI   R2,16
       JL   LSHR_LOOP
       ; Shift by 16: move high word to low, clear high
       MOV  R0,R1
       CLR  R0
       AI   R2,-16              ; Remaining shift
       JEQ  LSHR_RET

LSHR_LOOP:
       ; Shift R0:R1 right by 1 bit (logical)
       ; Need to capture LSB of R0 before shifting
       MOV  R0,R3               ; Save R0
       ANDI R3,1                ; Isolate LSB
       SRL  R0,1                ; Shift high word right
       SRL  R1,1                ; Shift low word right
       ; If R0's LSB was set, set R1's MSB
       MOV  R3,R3
       JEQ  LSHR_NC
       ORI  R1,>8000            ; Set MSB of low word
LSHR_NC:
       DEC  R2
       JNE  LSHR_LOOP

LSHR_RET:
       B    *R11

;======================================================================
; __ashrsi3 - 32-bit arithmetic right shift
; Input:  R0:R1 = value (high:low)
;         R2 = shift amount
; Output: R0:R1 = value >> shift_amount (sign extend)
;
; Algorithm: Loop shifting 1 bit at a time, preserving sign
;======================================================================
__ashrsi3:
       MOV  R2,R2               ; Check shift amount
       JEQ  ASHR_RET            ; If 0, return unchanged

       ; Check for shift >= 32
       CI   R2,32
       JL   ASHR_OK
       ; Return all 0s or all 1s based on sign
       MOV  R0,R0               ; Test sign
       JLT  ASHR_NEG
       CLR  R0                  ; Positive: return 0
       CLR  R1
       B    *R11
ASHR_NEG:
       SETO R0                  ; Negative: return -1
       SETO R1
       B    *R11

ASHR_OK:
       ; Check for shift >= 16 (word shift optimization)
       CI   R2,16
       JL   ASHR_LOOP
       ; Shift by 16: move high word to low, sign extend high
       MOV  R0,R1               ; High to low
       MOV  R0,R0               ; Test sign
       JLT  ASHR_SEXT
       CLR  R0                  ; Positive: clear high
       JMP  ASHR_CHK16
ASHR_SEXT:
       SETO R0                  ; Negative: all 1s in high
ASHR_CHK16:
       AI   R2,-16              ; Remaining shift
       JEQ  ASHR_RET

ASHR_LOOP:
       ; Shift R0:R1 right by 1 bit (arithmetic)
       ; Need to capture LSB of R0 before shifting
       MOV  R0,R3               ; Save R0
       ANDI R3,1                ; Isolate LSB
       SRA  R0,1                ; Shift high word right (sign preserving)
       SRL  R1,1                ; Shift low word right
       ; If R0's LSB was set, set R1's MSB
       MOV  R3,R3
       JEQ  ASHR_NC
       ORI  R1,>8000            ; Set MSB of low word
ASHR_NC:
       DEC  R2
       JNE  ASHR_LOOP

ASHR_RET:
       B    *R11

       END
