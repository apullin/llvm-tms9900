; ---------------------------------------------------------------------------
; Minimal 32-bit builtins needed by FreeRTOS kernel
;
; __mulsi3: 32-bit multiply
;   Input:  R0:R1 = A (high:low), R2:R3 = B (high:low)
;   Output: R0:R1 = product (lower 32 bits)
;
; Algorithm: (Ah*2^16 + Al) * (Bh*2^16 + Bl)
;   Lower 32 bits = Al*Bl + ((Ah*Bl + Al*Bh) << 16)
;
; Uses TMS9900 MPY instruction: MPY Rs,Rd -> Rd:Rd+1 = Rd * Rs (16x16->32)
; ---------------------------------------------------------------------------

        .text

        .global __mulsi3
__mulsi3:
        ; Save callee-saved register
        DECT    R10
        MOV     R9, *R10

        ; Save inputs
        MOV     R0, R9              ; R9 = Ah
        MOV     R1, R6              ; R6 = Al

        ; Step 1: Al * Bl -> R4:R5
        MOV     R6, R4
        MPY     R3, R4              ; R4:R5 = Al * Bl

        ; Step 2: Ah * Bl -> add low word to R4
        MOV     R9, R0
        MPY     R3, R0              ; R0:R1 = Ah * Bl
        A       R1, R4              ; R4 += low(Ah * Bl)

        ; Step 3: Al * Bh -> add low word to R4
        MOV     R6, R0
        MPY     R2, R0              ; R0:R1 = Al * Bh
        A       R1, R4              ; R4 += low(Al * Bh)

        ; Result
        MOV     R4, R0              ; R0 = high word
        MOV     R5, R1              ; R1 = low word

        ; Restore and return
        MOV     *R10+, R9
        B       *R11
