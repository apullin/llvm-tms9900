; libtms9900 - memcpy
; void *memcpy(void *dest, const void *src, size_t n)
;
; Input:  R0 = dest, R1 = src, R2 = n (byte count)
; Output: R0 = dest (preserved in R0 throughout)

       .text
       .global memcpy

memcpy:
       MOV  R2,R2
       JEQ  .Ldone

       MOV  R0,R3             ; R3 = working dest pointer
       MOV  R3,R4
       XOR  R1,R4
       ANDI R4,1
       JNE  .Lbyte_copy       ; different parity -> byte-by-byte

; Same alignment: copy leading odd byte to word-align if needed
       MOV  R3,R4
       ANDI R4,1
       JEQ  .Lwords
       MOVB *R1+,*R3+
       DEC  R2
       JEQ  .Ldone

.Lwords:
       MOV  R2,R4
       SRL  R4,1              ; R4 = word count, Carry = trailing odd byte
       JEQ  .Ltail_only

.Lword_loop:
       MOV  *R1+,*R3+
       DEC  R4
       JNE  .Lword_loop

.Ltail_only:
       ANDI R2,1
       JEQ  .Ldone
       MOVB *R1,*R3

.Ldone:
       B    *R11

.Lbyte_copy:
       MOVB *R1+,*R3+
       DEC  R2
       JNE  .Lbyte_copy
       B    *R11
