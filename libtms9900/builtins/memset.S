; libtms9900 - memset
; void *memset(void *dest, int c, size_t n)
;
; Input:  R0 = dest, R1 = fill char (low byte), R2 = n (byte count)
; Output: R0 = dest (original)
;
; Creates a word fill value (0xXXXX from 0x00XX) for word-aligned fills.
; Uses MOV R0, *R3+ to fill 2 bytes per iteration.

       .text
       .global memset

memset:
; Save original dest for return value
       MOV  R0,R4             ; R4 = original dest (return value)
       MOV  R0,R3             ; R3 = working dest pointer

; Early exit for zero length
       CI   R2,0
       JEQ  .Ldone

; Create word fill value: 0x00XX -> 0xXXXX
       MOV  R1,R0
       SWPB R0                ; R0 = 0xXX00
       SOC  R1,R0             ; R0 = 0xXXXX (OR in low byte)

; Handle leading odd byte to align
       MOV  R3,R5
       ANDI R5,1
       JEQ  .Lwords
       MOVB R0,*R3+           ; fill odd byte (MOVB stores high byte = XX)
       DEC  R2
       JEQ  .Ldone

; Word fill loop (2 bytes per iteration)
.Lwords:
       MOV  R2,R5
       SRL  R5,1              ; R5 = word count
       JEQ  .Ltail

.Lword_loop:
       MOV  R0,*R3+           ; fill word, dest advances by 2
       DEC  R5
       JNE  .Lword_loop

; Handle trailing odd byte
.Ltail:
       ANDI R2,1
       JEQ  .Ldone
       MOVB R0,*R3            ; fill last byte (MOVB stores high byte = XX)

.Ldone:
       MOV  R4,R0             ; return original dest
       B    *R11
