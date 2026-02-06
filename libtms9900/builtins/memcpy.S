; libtms9900 - memcpy
; void *memcpy(void *dest, const void *src, size_t n)
;
; Input:  R0 = dest, R1 = src, R2 = n (byte count)
; Output: R0 = dest (original)
;
; Optimized for TMS9900 auto-increment addressing:
;   MOV *R1+, *R3+   copies a word and advances both pointers by 2
;   MOVB *R1+, *R3+  copies a byte and advances both pointers by 1
;
; Handles:
;   - Word-aligned fast path (2 bytes per iteration)
;   - Byte fallback for misaligned src/dest
;   - Leading/trailing odd bytes

       .text
       .global memcpy

memcpy:
; Save original dest for return value
       MOV  R0,R4             ; R4 = original dest (return value)
       MOV  R0,R3             ; R3 = working dest pointer

; Early exit for zero length
       CI   R2,0
       JEQ  .Ldone

; Check if src and dest have same alignment parity
       MOV  R3,R0
       XOR  R1,R0
       ANDI R0,1
       JNE  .Lbyte_copy       ; different parity -> byte-by-byte

; Same alignment. Copy leading odd byte to align if needed
       MOV  R3,R0
       ANDI R0,1
       JEQ  .Lwords
       MOVB *R1+,*R3+         ; copy 1 byte, both advance by 1
       DEC  R2
       JEQ  .Ldone

; Word copy loop (2 bytes per iteration)
.Lwords:
       MOV  R2,R0
       SRL  R0,1              ; R0 = word count
       JEQ  .Ltail

.Lword_loop:
       MOV  *R1+,*R3+         ; copy word, both advance by 2
       DEC  R0
       JNE  .Lword_loop

; Handle trailing odd byte
.Ltail:
       ANDI R2,1
       JEQ  .Ldone
       MOVB *R1,*R3           ; copy last byte

.Ldone:
       MOV  R4,R0             ; return original dest
       B    *R11

; Byte-by-byte fallback (src/dest have different alignment)
.Lbyte_copy:
       MOVB *R1+,*R3+         ; copy byte, both advance by 1
       DEC  R2
       JNE  .Lbyte_copy
       JMP  .Ldone
