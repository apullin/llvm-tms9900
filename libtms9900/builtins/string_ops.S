; libtms9900 - Hand-tuned string/memory comparison routines
;
; Exploits TMS9900 byte auto-increment addressing (MOVB *Rn+,Rm and CB *R0+,*R1+).

       .text
       .global strlen
       .global strcmp
       .global memcmp

;----------------------------------------------------------------------
; size_t strlen(const char *s)
; Input:  R0 = s
; Output: R0 = length
;----------------------------------------------------------------------
strlen:
       MOV  R0,R1
.Lstrlen_loop:
       MOVB *R1+,R2
       JNE  .Lstrlen_loop
       DEC  R1
       S    R0,R1
       MOV  R1,R0
       B    *R11

;----------------------------------------------------------------------
; int strcmp(const char *s1, const char *s2)
; Input:  R0 = s1, R1 = s2
; Output: R0 = (int)(unsigned char)*s1 - (int)(unsigned char)*s2
;----------------------------------------------------------------------
strcmp:
.Lstrcmp_loop:
       MOVB *R0+,R2
       JEQ  .Lstrcmp_s1_end
       CB   R2,*R1+
       JEQ  .Lstrcmp_loop

       SRL  R2,8
       DEC  R1
       MOVB *R1,R0
       SRL  R0,8
       S    R0,R2
       MOV  R2,R0
       B    *R11

.Lstrcmp_s1_end:
       MOVB *R1,R0
       SRL  R0,8
       NEG  R0
       B    *R11

;----------------------------------------------------------------------
; int memcmp(const void *s1, const void *s2, size_t n)
; Input:  R0 = s1, R1 = s2, R2 = n
; Output: R0 = 0 if equal, else (int)*p1 - (int)*p2 at first mismatch
;----------------------------------------------------------------------
memcmp:
       MOV  R2,R2
       JEQ  .Lmemcmp_eq
.Lmemcmp_loop:
       CB   *R0+,*R1+
       JNE  .Lmemcmp_diff
       DEC  R2
       JNE  .Lmemcmp_loop
.Lmemcmp_eq:
       CLR  R0
       B    *R11

.Lmemcmp_diff:
       DEC  R0
       DEC  R1
       MOVB *R0,R0
       SRL  R0,8
       MOVB *R1,R1
       SRL  R1,8
       S    R1,R0
       B    *R11
