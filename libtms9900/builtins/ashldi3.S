; libtms9900 - 64-bit shift operations (hardware variable-shift accelerated)
;
; __ashldi3: Shift left  (64-bit)
; __lshrdi3: Logical shift right (64-bit, zero-filling)
; __ashrdi3: Arithmetic shift right (64-bit, sign-extending)
;
; Input:  R0:R1:R2:R3 = 64-bit value (high to low words)
;         *R10        = shift count (16-bit int on stack)
; Output: R0:R1:R2:R3 = shifted result
;
; Zero stack usage; uses only caller-saved R0-R8.

       .text
       .global __ashldi3
       .global __lshrdi3
       .global __ashrdi3

;----------------------------------------------------------------------
; __ashldi3 - 64-bit left shift
;----------------------------------------------------------------------
__ashldi3:
       MOV  *R10,R4
       JEQ  ASHL64_DONE
       CI   R4,64
       JHE  ASHL64_ZERO

       CI   R4,48
       JL   ASHL64_CHK32
       MOV  R3,R5
       CLR  R1
       CLR  R2
       CLR  R3
       AI   R4,-48
       JEQ  ASHL64_SET0
       MOV  R4,R0
       SLA  R5,0
ASHL64_SET0:
       MOV  R5,R0
       B    *R11

ASHL64_CHK32:
       CI   R4,32
       JL   ASHL64_CHK16
       MOV  R2,R0
       MOV  R3,R1
       CLR  R2
       CLR  R3
       AI   R4,-32
       JEQ  ASHL64_DONE
       JMP  ASHL64_SUB16

ASHL64_CHK16:
       CI   R4,16
       JL   ASHL64_SUB16
       MOV  R1,R0
       MOV  R2,R1
       MOV  R3,R2
       CLR  R3
       AI   R4,-16
       JEQ  ASHL64_DONE

ASHL64_SUB16:
; 1 <= R4 <= 15
       MOV  R0,R5
       MOV  R1,R6
       MOV  R2,R7
       MOV  R3,R8
       MOV  R4,R0             ; R0 = count (1..15)
       SLA  R5,0
       SLA  R6,0
       SLA  R7,0
       SLA  R8,0
       NEG  R0                ; R0[12:15] = 16 - count (1..15)
       SRL  R1,0
       SOC  R1,R5
       SRL  R2,0
       SOC  R2,R6
       SRL  R3,0
       SOC  R3,R7
       MOV  R5,R0
       MOV  R6,R1
       MOV  R7,R2
       MOV  R8,R3
ASHL64_DONE:
       B    *R11

ASHL64_ZERO:
       CLR  R0
       CLR  R1
       CLR  R2
       CLR  R3
       B    *R11

;----------------------------------------------------------------------
; __lshrdi3 - 64-bit logical right shift
;----------------------------------------------------------------------
__lshrdi3:
       MOV  *R10,R4
       JEQ  LSHR64_DONE
       CI   R4,64
       JHE  ASHL64_ZERO

       CI   R4,48
       JL   LSHR64_CHK32
       MOV  R0,R3
       CLR  R1
       CLR  R2
       AI   R4,-48
       JEQ  LSHR64_CLR0
       MOV  R4,R0
       SRL  R3,0
LSHR64_CLR0:
       CLR  R0
       B    *R11

LSHR64_CHK32:
       CI   R4,32
       JL   LSHR64_CHK16
       MOV  R1,R3
       MOV  R0,R2
       CLR  R0
       CLR  R1
       AI   R4,-32
       JEQ  LSHR64_DONE
       JMP  LSHR64_SUB16

LSHR64_CHK16:
       CI   R4,16
       JL   LSHR64_SUB16
       MOV  R2,R3
       MOV  R1,R2
       MOV  R0,R1
       CLR  R0
       AI   R4,-16
       JEQ  LSHR64_DONE

LSHR64_SUB16:
; 1 <= R4 <= 15
       MOV  R0,R5
       MOV  R0,R6             ; save W0 for << (16-c)
       MOV  R1,R7
       MOV  R2,R8
       MOV  R4,R0             ; R0 = count (1..15)
       SRL  R5,0              ; final R0
       SRL  R7,0
       SRL  R8,0
       SRL  R3,0
       NEG  R0                ; R0[12:15] = 16 - count (1..15)
       SLA  R6,0
       SOC  R6,R7             ; final R1
       SLA  R1,0
       SOC  R1,R8             ; final R2
       SLA  R2,0
       SOC  R2,R3             ; final R3
       MOV  R5,R0
       MOV  R7,R1
       MOV  R8,R2
LSHR64_DONE:
       B    *R11

;----------------------------------------------------------------------
; __ashrdi3 - 64-bit arithmetic right shift
;----------------------------------------------------------------------
__ashrdi3:
       MOV  *R10,R4
       JEQ  ASHR64_DONE
       CI   R4,64
       JHE  ASHR64_GE64

       MOV  R0,R5
       SRA  R5,15             ; R5 = sign fill (0 or 0xFFFF)

       CI   R4,48
       JL   ASHR64_CHK32
       MOV  R0,R3
       MOV  R5,R1
       MOV  R5,R2
       AI   R4,-48
       JEQ  ASHR64_SET0
       MOV  R4,R0
       SRA  R3,0
ASHR64_SET0:
       MOV  R5,R0
       B    *R11

ASHR64_CHK32:
       CI   R4,32
       JL   ASHR64_CHK16
       MOV  R1,R3
       MOV  R0,R2
       MOV  R5,R0
       MOV  R5,R1
       AI   R4,-32
       JEQ  ASHR64_DONE
       JMP  ASHR64_SUB16

ASHR64_CHK16:
       CI   R4,16
       JL   ASHR64_SUB16
       MOV  R2,R3
       MOV  R1,R2
       MOV  R0,R1
       MOV  R5,R0
       AI   R4,-16
       JEQ  ASHR64_DONE

ASHR64_SUB16:
; 1 <= R4 <= 15
       MOV  R0,R5
       MOV  R0,R6
       MOV  R1,R7
       MOV  R2,R8
       MOV  R4,R0             ; R0 = count (1..15)
       SRA  R5,0              ; final R0 (sign-extended)
       SRL  R7,0
       SRL  R8,0
       SRL  R3,0
       NEG  R0                ; R0[12:15] = 16 - count (1..15)
       SLA  R6,0
       SOC  R6,R7             ; final R1
       SLA  R1,0
       SOC  R1,R8             ; final R2
       SLA  R2,0
       SOC  R2,R3             ; final R3
       MOV  R5,R0
       MOV  R7,R1
       MOV  R8,R2
ASHR64_DONE:
       B    *R11

ASHR64_GE64:
       SRA  R0,15
       MOV  R0,R1
       MOV  R0,R2
       MOV  R0,R3
       B    *R11
