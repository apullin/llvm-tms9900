; libtms9900 - 32-bit shift operations (hardware variable-shift accelerated)
;
; __ashlsi3: Shift left (logical/arithmetic)
; __lshrsi3: Logical shift right (zero-filling)
; __ashrsi3: Arithmetic shift right (sign-extending)
;
; Input:  R0:R1 = value to shift (high:low)
;         R2    = shift count
; Output: R0:R1 = shifted result (high:low)
;
; Exploits TMS9900 Format-7 count=0 mode (uses R0[12:15] as shift count)
; and two's-complement identity (-c) & 15 == (16 - c) for 1 <= c <= 15.

       .text
       .global __ashlsi3
       .global __lshrsi3
       .global __ashrsi3

;----------------------------------------------------------------------
; __ashlsi3 - 32-bit left shift: R0:R1 <<= R2
;----------------------------------------------------------------------
__ashlsi3:
       MOV  R2,R2
       JEQ  ASHL_DONE
       CI   R2,32
       JHE  ASHL_ZERO
       CI   R2,16
       JHE  ASHL_GE16

; 1 <= count <= 15
       MOV  R0,R3             ; R3 = Hi
       MOV  R1,R4             ; R4 = Lo
       MOV  R2,R0             ; R0 = count (1..15)
       SLA  R3,0              ; R3 = Hi << count
       SLA  R4,0              ; R4 = Lo << count (final low word)
       NEG  R0                ; R0[12:15] = 16 - count (1..15)
       SRL  R1,0              ; R1 = Lo >> (16 - count)
       SOC  R1,R3             ; R3 |= Lo >> (16 - count)
       MOV  R3,R0
       MOV  R4,R1
       B    *R11

ASHL_GE16:
       MOV  R1,R3             ; new Hi = Lo
       CLR  R1                ; new Lo = 0
       AI   R2,-16
       JEQ  ASHL_EQ16
       MOV  R2,R0             ; R0 = count - 16 (1..15)
       SLA  R3,0
ASHL_EQ16:
       MOV  R3,R0
ASHL_DONE:
       B    *R11

ASHL_ZERO:
       CLR  R0
       CLR  R1
       B    *R11

;----------------------------------------------------------------------
; __lshrsi3 - 32-bit logical right shift: R0:R1 >>= R2
;----------------------------------------------------------------------
__lshrsi3:
       MOV  R2,R2
       JEQ  LSHR_DONE
       CI   R2,32
       JHE  ASHL_ZERO
       CI   R2,16
       JHE  LSHR_GE16

; 1 <= count <= 15
       MOV  R0,R3             ; R3 = Hi
       MOV  R0,R4             ; R4 = Hi
       MOV  R2,R0             ; R0 = count (1..15)
       SRL  R3,0              ; R3 = Hi >> count (final high word)
       SRL  R1,0              ; R1 = Lo >> count
       NEG  R0                ; R0[12:15] = 16 - count (1..15)
       SLA  R4,0              ; R4 = Hi << (16 - count)
       SOC  R4,R1             ; R1 |= Hi << (16 - count)
       MOV  R3,R0
       B    *R11

LSHR_GE16:
       MOV  R0,R1             ; new Lo = Hi
       AI   R2,-16
       JEQ  LSHR_EQ16
       MOV  R2,R0             ; R0 = count - 16 (1..15)
       SRL  R1,0
LSHR_EQ16:
       CLR  R0                ; new Hi = 0
LSHR_DONE:
       B    *R11

;----------------------------------------------------------------------
; __ashrsi3 - 32-bit arithmetic right shift: R0:R1 >>= R2 (signed)
;----------------------------------------------------------------------
__ashrsi3:
       MOV  R2,R2
       JEQ  ASHR_DONE
       CI   R2,32
       JHE  ASHR_GE32
       CI   R2,16
       JHE  ASHR_GE16

; 1 <= count <= 15
       MOV  R0,R3             ; R3 = Hi
       MOV  R0,R4             ; R4 = Hi
       MOV  R2,R0             ; R0 = count (1..15)
       SRA  R3,0              ; R3 = Hi >>a count (final high word)
       SRL  R1,0              ; R1 = Lo >>u count
       NEG  R0                ; R0[12:15] = 16 - count (1..15)
       SLA  R4,0              ; R4 = Hi << (16 - count)
       SOC  R4,R1             ; R1 |= Hi << (16 - count)
       MOV  R3,R0
       B    *R11

ASHR_GE16:
       MOV  R0,R1             ; new Lo = Hi
       MOV  R0,R3
       SRA  R3,15             ; R3 = sign fill (0 or 0xFFFF)
       AI   R2,-16
       JEQ  ASHR_EQ16
       MOV  R2,R0             ; R0 = count - 16 (1..15)
       SRA  R1,0
ASHR_EQ16:
       MOV  R3,R0
ASHR_DONE:
       B    *R11

ASHR_GE32:
       SRA  R0,15
       MOV  R0,R1
       B    *R11
