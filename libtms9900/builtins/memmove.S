; libtms9900 - memmove
; void *memmove(void *dest, const void *src, size_t n)
;
; Input:  R0 = dest, R1 = src, R2 = n (byte count)
; Output: R0 = dest (preserved in R0 throughout)
;
; Shares forward copy path with memcpy when dest < src.

       .text
       .global memmove
       .extern memcpy

memmove:
       MOV  R2,R2
       JEQ  .Ldone

       C    R0,R1             ; compare dest (R0) to src (R1)
       JL   .Lforward         ; dest < src -> forward copy via memcpy
       JEQ  .Ldone            ; dest == src -> no-op

; ---- Backward copy (dest > src) ----
       MOV  R0,R3
       A    R2,R3             ; R3 = dest + n
       A    R2,R1             ; R1 = src + n

       MOV  R3,R4
       XOR  R1,R4
       ANDI R4,1
       JNE  .Lback_byte       ; different parity -> byte-by-byte

; Handle trailing odd byte first
       MOV  R3,R4
       ANDI R4,1
       JEQ  .Lback_words
       DEC  R1
       DEC  R3
       MOVB *R1,*R3
       DEC  R2
       JEQ  .Ldone

.Lback_words:
       MOV  R2,R4
       SRL  R4,1              ; R4 = word count
       JEQ  .Lback_tail

.Lback_word_loop:
       DECT R1
       DECT R3
       MOV  *R1,*R3
       DEC  R4
       JNE  .Lback_word_loop

.Lback_tail:
       ANDI R2,1
       JEQ  .Ldone
       DEC  R1
       DEC  R3
       MOVB *R1,*R3

.Ldone:
       B    *R11

.Lback_byte:
       DEC  R1
       DEC  R3
       MOVB *R1,*R3
       DEC  R2
       JNE  .Lback_byte
       B    *R11

.Lforward:
       B    @memcpy
