; libtms9900 - 32-bit multiply
; __mulsi3: Multiply two 32-bit integers
;
; Input:  R0:R1 = first operand A (high:low)
;         R2:R3 = second operand B (high:low)
; Output: R0:R1 = product (high:low, lower 32 bits only)
;
; Algorithm: Long multiplication keeping only lower 32 bits
;   (Ah*2^16 + Al) * (Bh*2^16 + Bl) =
;   Ah*Bh*2^32 + (Ah*Bl + Al*Bh)*2^16 + Al*Bl
;
;   Lower 32 bits = Al*Bl + ((Ah*Bl + Al*Bh) << 16)
;   Which is: low(Al*Bl) in low word,
;             high(Al*Bl) + low(Ah*Bl) + low(Al*Bh) in high word
;
; TMS9900 MPY instruction: MPY Rs,Rd
;   Rd:Rd+1 = Rd * Rs (unsigned 16x16->32)
;   Rd must be even register (R0, R2, R4, R6, R8)
;
; Register plan:
;   Entry: R0=Ah, R1=Al, R2=Bh, R3=Bl
;   Use R4:R5 for MPY workspace
;   Use R6, R7 for temp saves
;   Result: R0=high, R1=low

       .text
       .global __mulsi3

__mulsi3:
; Save callee-saved registers
       DECT R10
       MOV  R9,*R10           ; Push R9

; Save inputs we'll need later
       MOV  R0,R9             ; R9 = Ah
       MOV  R1,R6             ; R6 = Al (we'll clobber R1)

; Step 1: Al * Bl -> full 32-bit result in R4:R5
; (Uses R0 as MPY destination since we need R0:R1 result anyway)
       MOV  R6,R0             ; R0 = Al
       MPY  R3,R0             ; R0:R1 = Al * Bl
       MOV  R0,R4             ; R4 = high(Al * Bl)
       MOV  R1,R5             ; R5 = low(Al * Bl)

; Step 2: Ah * Bl -> add low word to R4
       MOV  R9,R0             ; R0 = Ah
       MPY  R3,R0             ; R0:R1 = Ah * Bl
       A    R1,R4             ; R4 += low(Ah * Bl)

; Step 3: Al * Bh -> add low word to R4
       MOV  R6,R0             ; R0 = Al
       MPY  R2,R0             ; R0:R1 = Al * Bh
       A    R1,R4             ; R4 += low(Al * Bh)

; Move result to R0:R1
       MOV  R4,R0             ; R0 = high word
       MOV  R5,R1             ; R1 = low word

; Restore and return
       MOV  *R10+,R9          ; Pop R9
       B    *R11
