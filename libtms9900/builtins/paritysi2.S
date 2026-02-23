; libtms9900 - Parity operations
;
; __paritysi2: Parity of 32-bit value (1 if odd number of bits set)
; __paritydi2: Parity of 64-bit value
;
; __paritysi2:
;   Input:  R0:R1 = 32-bit value (high:low)
;   Output: R0 = 1 if odd parity, 0 if even
;
; __paritydi2:
;   Input:  R0:R1:R2:R3 = 64-bit value
;   Output: R0 = 1 if odd parity, 0 if even
;
; Algorithm: XOR-fold to 4 bits, then use lookup table 0x6996.
;   For 32-bit: XOR high^low -> 16 bits, fold 16->8->4, lookup
;   For 64-bit: XOR all 4 words first, then same as 32-bit on one word
;
; TMS9900 note: JOP (jump on odd parity) checks parity of high byte
; of last result. We can exploit this!
; Actually, JOP checks the OP status bit which is set by various
; instructions based on odd parity of the result byte. Let's use
; the XOR-fold + lookup approach for clarity and correctness.

       .text
       .global __paritysi2
       .global __paritydi2

;----------------------------------------------------------------------
; __paritydi2 - 64-bit parity
; XOR all 4 words, then fall through to 16-bit parity
;----------------------------------------------------------------------
__paritydi2:
       XOR  R1,R0              ; R0 ^= R1
       XOR  R2,R0              ; R0 ^= R2
       XOR  R3,R0              ; R0 ^= R3
       JMP  PARITY16           ; now do parity of R0 (16 bits)

;----------------------------------------------------------------------
; __paritysi2 - 32-bit parity
; XOR two words, then do 16-bit parity
;----------------------------------------------------------------------
__paritysi2:
       XOR  R1,R0              ; R0 ^= R1 (fold 32->16)

PARITY16:
; Fold 16 -> 8 bits
       MOV  R0,R1
       SWPB R1                 ; swap bytes: high byte -> low byte position
       XOR  R1,R0              ; R0 low byte has 8-bit parity info
; Fold 8 -> 4 bits
       MOV  R0,R1
       SRL  R1,4               ; R1 bits 11:8 = R0 bits 7:4
       XOR  R1,R0              ; R0 bits 3:0 have 4-bit parity
       ANDI R0,0x000F          ; mask to 4 bits
; Lookup in 0x6996 table
; Bit N of 0x6996 = parity of N
; We need (0x6996 >> R0) & 1
; TMS9900 SRL uses R0 as count when count field is 0, but R0 is our index!
; So we must put the table in another register.
       MOV  R0,R1              ; R1 = index (0-15)
       LI   R0,0x6996          ; lookup table
; SRL R0,count — but count must be in R0 when count field is 0.
; We need SRL with variable count. SRL Rd,0 means shift by R0.
; So swap: put count in R0, table in R1
; Wait, SRL Rd,0 shifts Rd by the value in R0 (bits 12-15).
; So: R0=count, R1=table; SRL R1,0 shifts R1 by R0.
       MOV  R1,R0              ; R0 = shift count
       LI   R1,0x6996
       SRL  R1,0               ; R1 >>= R0 (variable shift by R0)
       MOV  R1,R0
       ANDI R0,1               ; isolate bit 0
       B    *R11
