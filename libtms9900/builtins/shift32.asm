* libtms9900 - 32-bit shift operations
*
* __ashlsi3: Shift left (logical/arithmetic - same for left shift)
* __ashrsi3: Arithmetic shift right (preserves sign)
* __lshrsi3: Logical shift right (fills with zeros)
*
* Input:  R0:R1 = value to shift (high:low)
*         R2 = shift count (0-31, counts >= 32 result in 0 or -1)
* Output: R0:R1 = shifted result (high:low)
*
* TMS9900 SLA/SRA/SRL instructions can shift 0-15 bits.
* For 32-bit shifts, we need to handle bit transfer between words.

       DEF  __ashlsi3
       DEF  __ashrsi3
       DEF  __lshrsi3

*----------------------------------------------------------------------
* __ashlsi3 - 32-bit left shift
* R0:R1 <<= R2
*----------------------------------------------------------------------
__ashlsi3:
* Handle shift count >= 32
       CI   R2,32
       JL   ASHL_OK
       CLR  R0
       CLR  R1
       B    *R11
ASHL_OK:
       MOV  R2,R2
       JEQ  ASHL_DONE         ; Shift by 0, nothing to do

* If shift >= 16, move low to high and clear low
       CI   R2,16
       JL   ASHL_LOOP
       MOV  R1,R0             ; High = low
       CLR  R1                ; Low = 0
       AI   R2,-16            ; Reduce count by 16
       JEQ  ASHL_DONE         ; If exactly 16, done

ASHL_LOOP:
* Shift one bit at a time
* Shift R1 left, carry out goes to R0
       SLA  R1,1              ; Shift low word left
       JNC  ASHL_NC
       INC  R0                ; Set bit 0 of high word
ASHL_NC:
       SLA  R0,1              ; Shift high word left
       DEC  R2
       JNE  ASHL_LOOP

ASHL_DONE:
       B    *R11

*----------------------------------------------------------------------
* __lshrsi3 - 32-bit logical right shift
* R0:R1 >>= R2 (unsigned, fills with 0)
*----------------------------------------------------------------------
__lshrsi3:
* Handle shift count >= 32
       CI   R2,32
       JL   LSHR_OK
       CLR  R0
       CLR  R1
       B    *R11
LSHR_OK:
       MOV  R2,R2
       JEQ  LSHR_DONE         ; Shift by 0, nothing to do

* If shift >= 16, move high to low and clear high
       CI   R2,16
       JL   LSHR_LOOP
       MOV  R0,R1             ; Low = high
       CLR  R0                ; High = 0
       AI   R2,-16            ; Reduce count by 16
       JEQ  LSHR_DONE         ; If exactly 16, done

LSHR_LOOP:
* Shift one bit at a time
* Shift R0 right, carry out goes to R1
       SRL  R0,1              ; Shift high word right
       JNC  LSHR_NC
       ORI  R1,>8000          ; Set bit 15 of low word
LSHR_NC:
       SRL  R1,1              ; Shift low word right
       DEC  R2
       JNE  LSHR_LOOP

LSHR_DONE:
       B    *R11

*----------------------------------------------------------------------
* __ashrsi3 - 32-bit arithmetic right shift
* R0:R1 >>= R2 (signed, fills with sign bit)
*----------------------------------------------------------------------
__ashrsi3:
* Handle shift count >= 32
       CI   R2,32
       JL   ASHR_OK
* Result is 0 or -1 depending on sign
       MOV  R0,R0
       JLT  ASHR_NEG
       CLR  R0
       CLR  R1
       B    *R11
ASHR_NEG:
       SETO R0                ; Fill with 1s
       SETO R1
       B    *R11

ASHR_OK:
       MOV  R2,R2
       JEQ  ASHR_DONE         ; Shift by 0, nothing to do

* If shift >= 16, do arithmetic shift of high to low, sign-extend high
       CI   R2,16
       JL   ASHR_LOOP
       MOV  R0,R1             ; Low = high
       MOV  R0,R0             ; Check sign of original high
       JLT  ASHR_NEG16
       CLR  R0                ; High = 0 (was positive)
       JMP  ASHR_CONT
ASHR_NEG16:
       SETO R0                ; High = -1 (was negative)
ASHR_CONT:
       AI   R2,-16            ; Reduce count by 16
       JEQ  ASHR_DONE         ; If exactly 16, done

ASHR_LOOP:
* Shift one bit at a time, preserving sign
* Use SRA for high word (preserves sign bit)
       SRA  R0,1              ; Arithmetic shift high word right
       JNC  ASHR_NC
       ORI  R1,>8000          ; Set bit 15 of low word
ASHR_NC:
       SRL  R1,1              ; Logical shift low word right
       DEC  R2
       JNE  ASHR_LOOP

ASHR_DONE:
       B    *R11

       END
