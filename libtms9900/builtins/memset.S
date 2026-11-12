; libtms9900 - memset
; void *memset(void *dest, int c, size_t n)
;
; Input:  R0 = dest, R1 = fill char, R2 = n (byte count)
; Output: R0 = dest (preserved in R0 throughout)

       .text
       .global memset

memset:
       MOV  R2,R2
       JEQ  .Ldone

       MOV  R0,R3             ; R3 = working dest pointer
       ANDI R1,0x00FF         ; C requires unsigned char conversion
       MOV  R1,R4
       SWPB R4                ; R4 = 0xXX00
       SOC  R1,R4             ; R4 = 0xXXXX

; Handle leading odd byte to word-align
       MOV  R3,R5
       ANDI R5,1
       JEQ  .Lwords
       MOVB R4,*R3+
       DEC  R2
       JEQ  .Ldone

.Lwords:
       MOV  R2,R5
       SRL  R5,1              ; R5 = word count
       JEQ  .Ltail

.Lword_loop:
       MOV  R4,*R3+
       DEC  R5
       JNE  .Lword_loop

.Ltail:
       ANDI R2,1
       JEQ  .Ldone
       MOVB R4,*R3

.Ldone:
       B    *R11
