; libtms9900 - 32-bit multiply
; __mulsi3: Multiply two 32-bit integers
;
; Input:  R0:R1 = first operand A (high:low)
;         R2:R3 = second operand B (high:low)
; Output: R0:R1 = product (high:low, lower 32 bits)
;
; Algorithm:
;   low32(A * B) = Al*Bl + ((low16(Ah*Bl) + low16(Bh*Al)) << 16)
;
; Uses only caller-saved registers R0-R6 (zero stack usage).

       .text
       .global __mulsi3

__mulsi3:
       MOV  R0,R4             ; R4 = Ah
       MPY  R3,R4             ; R4:R5 = Ah * Bl -> R5 = low16(Ah * Bl)
       MOV  R1,R0             ; R0 = Al
       MOV  R1,R6             ; R6 = Al
       MPY  R3,R0             ; R0:R1 = Al * Bl -> R0 = high16(Al*Bl), R1 = final low word
       MPY  R6,R2             ; R2:R3 = Bh * Al -> R3 = low16(Bh * Al)
       A    R5,R0             ; R0 += low16(Ah * Bl)
       A    R3,R0             ; R0 += low16(Bh * Al)
       B    *R11
