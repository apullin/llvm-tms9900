; libtms9900 - memmove
; void *memmove(void *dest, const void *src, size_t n)
;
; Input:  R0 = dest, R1 = src, R2 = n (byte count)
; Output: R0 = dest (original)
;
; Overlap-safe memory copy:
;   dest < src  -> forward copy (same as memcpy)
;   dest > src  -> backward copy (start from end)
;   dest == src -> no-op
;
; TMS9900 has auto-increment (*R+) but NOT auto-decrement,
; so backward copy uses manual DECT/DEC for pointer adjustment.

       .text
       .global memmove

memmove:
; Save original dest for return value
       MOV  R0,R4             ; R4 = original dest (return value)

; Early exit for zero length
       CI   R2,0
       JEQ  .Ldone

; Determine copy direction
       C    R0,R1             ; compare dest to src
       JL   .Lforward         ; dest < src -> forward is safe
       JEQ  .Ldone            ; dest == src -> nothing to do

; ---- Backward copy (dest > src) ----
; Set pointers to one past the end
       MOV  R0,R3
       A    R2,R3             ; R3 = dest + n
       A    R2,R1             ; R1 = src + n

; Check alignment parity for backward word copy
       MOV  R3,R0
       XOR  R1,R0
       ANDI R0,1
       JNE  .Lback_byte       ; different parity -> byte-by-byte

; Handle trailing odd byte (at the end, copy first in backward)
       MOV  R3,R0
       ANDI R0,1
       JEQ  .Lback_words
       DEC  R1
       DEC  R3
       MOVB *R1,*R3           ; copy last byte
       DEC  R2
       JEQ  .Ldone

; Backward word copy loop
.Lback_words:
       MOV  R2,R0
       SRL  R0,1              ; R0 = word count
       JEQ  .Lback_tail

.Lback_word_loop:
       DECT R1                ; back up src by 2
       DECT R3                ; back up dest by 2
       MOV  *R1,*R3           ; copy word (register indirect, no auto-inc)
       DEC  R0
       JNE  .Lback_word_loop

; Handle leading odd byte
.Lback_tail:
       ANDI R2,1
       JEQ  .Ldone
       DEC  R1
       DEC  R3
       MOVB *R1,*R3
       JMP  .Ldone

; Backward byte-by-byte
.Lback_byte:
       DEC  R1
       DEC  R3
       MOVB *R1,*R3
       DEC  R2
       JNE  .Lback_byte
       JMP  .Ldone

; ---- Forward copy (dest < src, same as memcpy) ----
.Lforward:
       MOV  R0,R3             ; R3 = working dest

; Check alignment parity
       MOV  R3,R0
       XOR  R1,R0
       ANDI R0,1
       JNE  .Lfwd_byte        ; different parity -> byte-by-byte

; Handle leading odd byte
       MOV  R3,R0
       ANDI R0,1
       JEQ  .Lfwd_words
       MOVB *R1+,*R3+
       DEC  R2
       JEQ  .Ldone

; Forward word copy loop
.Lfwd_words:
       MOV  R2,R0
       SRL  R0,1
       JEQ  .Lfwd_tail

.Lfwd_word_loop:
       MOV  *R1+,*R3+
       DEC  R0
       JNE  .Lfwd_word_loop

; Handle trailing odd byte
.Lfwd_tail:
       ANDI R2,1
       JEQ  .Ldone
       MOVB *R1,*R3

.Ldone:
       MOV  R4,R0             ; return original dest
       B    *R11

; Forward byte-by-byte
.Lfwd_byte:
       MOVB *R1+,*R3+
       DEC  R2
       JNE  .Lfwd_byte
       JMP  .Ldone
