; libtms9900 - 64-bit shift operations
;
; __ashldi3: Shift left  (64-bit)
; __ashrdi3: Arithmetic shift right (64-bit, sign-extending)
; __lshrdi3: Logical shift right (64-bit, zero-filling)
;
; Input:  R0:R1:R2:R3 = 64-bit value (high to low words)
;         *R10 = shift count (int, on stack)
; Output: R0:R1:R2:R3 = shifted result

       .text
       .global __ashldi3
       .global __ashrdi3
       .global __lshrdi3

;----------------------------------------------------------------------
; __ashldi3 - 64-bit left shift
; R0:R1:R2:R3 <<= count
;----------------------------------------------------------------------
__ashldi3:
       DECT R10
       MOV  R11,*R10
       MOV  @2(R10),R4         ; R4 = shift count
; Handle shift count >= 64
       CI   R4,64
       JL   ASHL64_OK
       CLR  R0
       CLR  R1
       CLR  R2
       CLR  R3
       MOV  *R10+,R11
       B    *R11
ASHL64_OK:
       MOV  R4,R4
       JEQ  ASHL64_DONE        ; shift by 0 -> nothing to do

; If shift >= 48: R0 = R3, rest = 0, then shift remaining
       CI   R4,48
       JL   ASHL64_CHK32
       MOV  R3,R0
       CLR  R1
       CLR  R2
       CLR  R3
       AI   R4,-48
       JEQ  ASHL64_DONE
       JMP  ASHL64_LOOP

; If shift >= 32: move words up by 2
ASHL64_CHK32:
       CI   R4,32
       JL   ASHL64_CHK16
       MOV  R2,R0
       MOV  R3,R1
       CLR  R2
       CLR  R3
       AI   R4,-32
       JEQ  ASHL64_DONE
       JMP  ASHL64_LOOP

; If shift >= 16: move words up by 1
ASHL64_CHK16:
       CI   R4,16
       JL   ASHL64_LOOP
       MOV  R1,R0
       MOV  R2,R1
       MOV  R3,R2
       CLR  R3
       AI   R4,-16
       JEQ  ASHL64_DONE

ASHL64_LOOP:
; Shift one bit left, processing from HIGH to LOW.
; This ensures carry from SLA is checked before INC can clobber it.
       SLA  R0,1               ; shift R0 left (makes room at bit 0)
       SLA  R1,1               ; shift R1 left; carry = old MSB of R1
       JNC  ASHL64_NC1
       INC  R0                 ; propagate carry from R1 to R0
ASHL64_NC1:
       SLA  R2,1               ; shift R2 left; carry = old MSB of R2
       JNC  ASHL64_NC2
       INC  R1                 ; propagate carry from R2 to R1
ASHL64_NC2:
       SLA  R3,1               ; shift R3 left; carry = old MSB of R3
       JNC  ASHL64_NC3
       INC  R2                 ; propagate carry from R3 to R2
ASHL64_NC3:
       DEC  R4
       JNE  ASHL64_LOOP

ASHL64_DONE:
       MOV  *R10+,R11
       B    *R11

;----------------------------------------------------------------------
; __lshrdi3 - 64-bit logical right shift
; R0:R1:R2:R3 >>= count (unsigned, fills with 0)
;----------------------------------------------------------------------
__lshrdi3:
       DECT R10
       MOV  R11,*R10
       MOV  @2(R10),R4         ; R4 = shift count
; Handle shift count >= 64
       CI   R4,64
       JL   LSHR64_OK
       CLR  R0
       CLR  R1
       CLR  R2
       CLR  R3
       MOV  *R10+,R11
       B    *R11
LSHR64_OK:
       MOV  R4,R4
       JEQ  LSHR64_DONE

; If shift >= 48
       CI   R4,48
       JL   LSHR64_CHK32
       MOV  R0,R3
       CLR  R0
       CLR  R1
       CLR  R2
       AI   R4,-48
       JEQ  LSHR64_DONE
       JMP  LSHR64_LOOP

; If shift >= 32
LSHR64_CHK32:
       CI   R4,32
       JL   LSHR64_CHK16
       MOV  R1,R3
       MOV  R0,R2
       CLR  R0
       CLR  R1
       AI   R4,-32
       JEQ  LSHR64_DONE
       JMP  LSHR64_LOOP

; If shift >= 16
LSHR64_CHK16:
       CI   R4,16
       JL   LSHR64_LOOP
       MOV  R2,R3
       MOV  R1,R2
       MOV  R0,R1
       CLR  R0
       AI   R4,-16
       JEQ  LSHR64_DONE

LSHR64_LOOP:
; Shift one bit right, processing from LOW to HIGH.
; SRL doesn't set carry in the same way as SLA. On TMS9900,
; SRL sets carry = the last bit shifted out (bit 0 before shift).
; ORI does NOT affect carry, so carry is preserved across ORI.
       SRL  R3,1               ; shift R3 right (makes room at bit 15)
       SRL  R2,1               ; shift R2 right; carry = old LSB of R2
       JNC  LSHR64_NC2
       ORI  R3,0x8000          ; set MSB of R3 from R2's old LSB
LSHR64_NC2:
       SRL  R1,1               ; carry = old LSB of R1
       JNC  LSHR64_NC1
       ORI  R2,0x8000          ; set MSB of R2 from R1's old LSB
LSHR64_NC1:
       SRL  R0,1               ; carry = old LSB of R0
       JNC  LSHR64_NC0
       ORI  R1,0x8000          ; set MSB of R1 from R0's old LSB
LSHR64_NC0:
       DEC  R4
       JNE  LSHR64_LOOP

LSHR64_DONE:
       MOV  *R10+,R11
       B    *R11

;----------------------------------------------------------------------
; __ashrdi3 - 64-bit arithmetic right shift
; R0:R1:R2:R3 >>= count (signed, fills with sign bit)
;----------------------------------------------------------------------
__ashrdi3:
       DECT R10
       MOV  R11,*R10
       MOV  @2(R10),R4         ; R4 = shift count
; Handle shift count >= 64
       CI   R4,64
       JL   ASHR64_OK
; Result is 0 or -1 depending on sign
       MOV  R0,R0
       JLT  ASHR64_NEG
       CLR  R0
       CLR  R1
       CLR  R2
       CLR  R3
       MOV  *R10+,R11
       B    *R11
ASHR64_NEG:
       SETO R0
       SETO R1
       SETO R2
       SETO R3
       MOV  *R10+,R11
       B    *R11

ASHR64_OK:
       MOV  R4,R4
       JEQ  ASHR64_DONE

; If shift >= 48
       CI   R4,48
       JL   ASHR64_CHK32
       MOV  R0,R3
       SRA  R0,15              ; sign extend R0
       MOV  R0,R1
       MOV  R0,R2
       AI   R4,-48
       JEQ  ASHR64_DONE
       JMP  ASHR64_LOOP

; If shift >= 32
ASHR64_CHK32:
       CI   R4,32
       JL   ASHR64_CHK16
       MOV  R1,R3
       MOV  R0,R2
       SRA  R0,15              ; sign extend
       MOV  R0,R1
       AI   R4,-32
       JEQ  ASHR64_DONE
       JMP  ASHR64_LOOP

; If shift >= 16
ASHR64_CHK16:
       CI   R4,16
       JL   ASHR64_LOOP
       MOV  R2,R3
       MOV  R1,R2
       MOV  R0,R1
       SRA  R0,15              ; sign extend into word0
       AI   R4,-16
       JEQ  ASHR64_DONE

ASHR64_LOOP:
; Arithmetic right shift one bit, processing from LOW to HIGH.
; SRA on R0 preserves sign, SRL on others fills with 0.
; Carry from SRA/SRL = old LSB. ORI doesn't affect carry.
       SRL  R3,1               ; shift R3 right
       SRL  R2,1               ; carry = old LSB of R2
       JNC  ASHR64_NC2
       ORI  R3,0x8000          ; set MSB of R3
ASHR64_NC2:
       SRL  R1,1               ; carry = old LSB of R1
       JNC  ASHR64_NC1
       ORI  R2,0x8000
ASHR64_NC1:
       SRA  R0,1               ; arithmetic shift R0; carry = old LSB of R0
       JNC  ASHR64_NC0
       ORI  R1,0x8000          ; set MSB of R1
ASHR64_NC0:
       DEC  R4
       JNE  ASHR64_LOOP

ASHR64_DONE:
       MOV  *R10+,R11
       B    *R11
