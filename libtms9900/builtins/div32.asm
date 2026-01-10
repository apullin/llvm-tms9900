* libtms9900 - 32-bit division
*
* __udivsi3: Unsigned 32-bit divide
* __divsi3:  Signed 32-bit divide
* __umodsi3: Unsigned 32-bit remainder
* __modsi3:  Signed 32-bit remainder
*
* Input:  R0:R1 = dividend (high:low)
*         R2:R3 = divisor (high:low)
* Output: R0:R1 = quotient (div) or remainder (mod)
*
* Algorithm: Shift-and-subtract (restoring division)
*   We treat remainder:quotient as a 64-bit shift register.
*   For each of 32 iterations:
*     1. Shift the 64-bit value left by 1
*     2. If remainder >= divisor, subtract divisor and set quotient LSB

       DEF  __udivsi3
       DEF  __umodsi3
       DEF  __divsi3
       DEF  __modsi3

*----------------------------------------------------------------------
* __udivsi3 - Unsigned 32-bit division
* Returns quotient in R0:R1
*----------------------------------------------------------------------
__udivsi3:
       DECT R10
       MOV  R11,*R10          ; Save return address
       BL   @UDIV32           ; Do the division
       MOV  R4,R0             ; Quotient high
       MOV  R5,R1             ; Quotient low
       MOV  *R10+,R11
       B    *R11

*----------------------------------------------------------------------
* __umodsi3 - Unsigned 32-bit remainder
* Returns remainder in R0:R1
*----------------------------------------------------------------------
__umodsi3:
       DECT R10
       MOV  R11,*R10          ; Save return address
       BL   @UDIV32           ; Do the division
       MOV  R6,R0             ; Remainder high
       MOV  R7,R1             ; Remainder low
       MOV  *R10+,R11
       B    *R11

*----------------------------------------------------------------------
* __divsi3 - Signed 32-bit division
* Returns quotient in R0:R1
*----------------------------------------------------------------------
__divsi3:
       DECT R10
       MOV  R11,*R10
       DECT R10
       MOV  R9,*R10           ; Save R9 for sign flag

       CLR  R9                ; R9 = sign flag (0 = positive result)

* Check if dividend is negative
       MOV  R0,R0
       JGE  DIV_D1
       INV  R9                ; Flip sign flag
       BL   @NEG32_01         ; Negate R0:R1
DIV_D1:
* Check if divisor is negative
       MOV  R2,R2
       JGE  DIV_D2
       INV  R9                ; Flip sign flag
       BL   @NEG32_23         ; Negate R2:R3
DIV_D2:
* Do unsigned division
       BL   @UDIV32
       MOV  R4,R0             ; Quotient to R0:R1
       MOV  R5,R1

* Apply sign to result
       MOV  R9,R9
       JEQ  DIV_D3
       BL   @NEG32_01         ; Negate result if signs differed
DIV_D3:
       MOV  *R10+,R9
       MOV  *R10+,R11
       B    *R11

*----------------------------------------------------------------------
* __modsi3 - Signed 32-bit remainder
* Returns remainder in R0:R1
* Remainder has same sign as dividend (C99 semantics)
*----------------------------------------------------------------------
__modsi3:
       DECT R10
       MOV  R11,*R10
       DECT R10
       MOV  R9,*R10           ; Save R9 for sign flag

       CLR  R9                ; R9 = sign of dividend

* Check if dividend is negative
       MOV  R0,R0
       JGE  MOD_D1
       SETO R9                ; Remember dividend was negative
       BL   @NEG32_01         ; Negate R0:R1
MOD_D1:
* Check if divisor is negative
       MOV  R2,R2
       JGE  MOD_D2
       BL   @NEG32_23         ; Negate R2:R3 (don't change sign flag)
MOD_D2:
* Do unsigned division
       BL   @UDIV32
       MOV  R6,R0             ; Remainder to R0:R1
       MOV  R7,R1

* Apply dividend's sign to remainder
       MOV  R9,R9
       JEQ  MOD_D3
       BL   @NEG32_01         ; Negate remainder if dividend was negative
MOD_D3:
       MOV  *R10+,R9
       MOV  *R10+,R11
       B    *R11

*----------------------------------------------------------------------
* UDIV32 - Core unsigned division routine
* Input:  R0:R1 = dividend
*         R2:R3 = divisor
* Output: R4:R5 = quotient
*         R6:R7 = remainder
* Clobbers: R0, R1, R8
*
* Algorithm:
*   remainder = 0
*   quotient = dividend
*   for i = 0 to 31:
*     shift remainder:quotient left by 1 (64-bit shift)
*     if remainder >= divisor:
*       remainder -= divisor
*       quotient |= 1  (set LSB)
*----------------------------------------------------------------------
UDIV32:
       DECT R10
       MOV  R11,*R10
       DECT R10
       MOV  R8,*R10           ; Save R8

* Initialize remainder to 0
       CLR  R6                ; Remainder high
       CLR  R7                ; Remainder low

* Initialize quotient to dividend
       MOV  R0,R4             ; Quotient high
       MOV  R1,R5             ; Quotient low

* Loop counter
       LI   R8,32             ; 32 bits to process

UDIV_LOOP:
* Shift 64-bit remainder:quotient left by 1
* Order: R6:R7:R4:R5 is the 64-bit value
* MSB of R4 goes into LSB of R7

* First, get the carry bit (MSB of R4) into a temp
       MOV  R4,R0             ; Copy quotient high
       SLA  R0,1              ; Shift to get MSB into carry
       JNC  UDIV_NC1
* Carry is set - MSB of quotient was 1
       SLA  R5,1              ; Shift quotient low
       SLA  R4,1              ; Shift quotient high
       SLA  R7,1              ; Shift remainder low
       INC  R7                ; Set LSB (the carry from quotient)
       JNC  UDIV_NC2
       INC  R6                ; Carry from R7 into R6
UDIV_NC2:
       SLA  R6,1              ; Shift remainder high
       JMP  UDIV_CMP

UDIV_NC1:
* Carry is clear - MSB of quotient was 0
       SLA  R5,1              ; Shift quotient low
       JNC  UDIV_NC3
       INC  R4                ; Carry from R5 bit 15 into R4 bit 0
UDIV_NC3:
       SLA  R4,1              ; Shift quotient high
       SLA  R7,1              ; Shift remainder low (bit from quotient is 0)
       JNC  UDIV_NC4
       INC  R6                ; Carry from R7 into R6
UDIV_NC4:
       SLA  R6,1              ; Shift remainder high

UDIV_CMP:
* Compare remainder (R6:R7) with divisor (R2:R3)
       C    R6,R2             ; Compare high words
       JL   UDIV_NEXT         ; remainder < divisor, skip
       JH   UDIV_SUB          ; remainder > divisor, subtract
       C    R7,R3             ; High equal, compare low
       JL   UDIV_NEXT         ; remainder < divisor, skip

UDIV_SUB:
* Subtract divisor from remainder: R6:R7 -= R2:R3
       S    R3,R7             ; Low word
       JOC  UDIV_NB           ; Jump if no borrow (carry set = no borrow)
       DEC  R6                ; Borrow from high word
UDIV_NB:
       S    R2,R6             ; High word
* Set LSB of quotient
       ORI  R5,1

UDIV_NEXT:
       DEC  R8
       JNE  UDIV_LOOP

       MOV  *R10+,R8          ; Restore R8
       MOV  *R10+,R11
       B    *R11

*----------------------------------------------------------------------
* NEG32_01 - Negate 32-bit value in R0:R1
* Two's complement: invert all bits, add 1
*----------------------------------------------------------------------
NEG32_01:
       INV  R1                ; Invert low word
       INV  R0                ; Invert high word
       INC  R1                ; Add 1 to low
       JNC  NEG01_DONE        ; No carry, done
       INC  R0                ; Carry to high word
NEG01_DONE:
       B    *R11

*----------------------------------------------------------------------
* NEG32_23 - Negate 32-bit value in R2:R3
*----------------------------------------------------------------------
NEG32_23:
       INV  R3
       INV  R2
       INC  R3
       JNC  NEG23_DONE
       INC  R2
NEG23_DONE:
       B    *R11

       END
