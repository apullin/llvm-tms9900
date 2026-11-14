; Exemplar 05: TMS9900 Workspace-Swapping (BLWP) Coroutines & Inline-DATA Service Linkage
; Unique to the TMS9900 architecture: 34-cycle context switches with zero register copying.

    .bss
    .p2align 1
service_ws:
    .space 32
coro_ws:
    .space 32

    .data
    .p2align 1
    .globl ex_add_scale_vec
    .globl ex_fib_gen_vec
ex_add_scale_vec:
    .short service_ws, ex_add_scale_entry
ex_fib_gen_vec:
    .short coro_ws, ex_fib_gen_entry

    .text
    .p2align 1

; Invoked via:
;   BLWP @ex_add_scale_vec
;   .short a, b, scale
; Fetches inline DATA words via *R14+ (advancing caller PC!) and writes caller R0 via *R13.
ex_add_scale_entry:
    MOV  *R14+, R0          ; fetch `a`, caller PC += 2
    A    *R14+, R0          ; add `b`, caller PC += 2
    MPY  *R14+, R0          ; multiply by `scale` -> R0:R1, caller PC += 2
    MOV  R1, *R13           ; store low 16 bits into caller's R0!
    RTWP

; Stateful Fibonacci generator in `coro_ws`:
;   Caller R1 != 0 -> reset state (R4=0, R5=1) and return 0 in caller R0
;   Caller R1 == 0 -> return R5 in caller R0 and advance (R4, R5) = (R5, R4 + R5)
; R4 and R5 persist permanently inside `coro_ws` across RTWP with zero memory saves!
ex_fib_gen_entry:
    MOV  @2(R13), R0        ; inspect caller's R1
    JEQ  .Lnext_fib
    CLR  R4
    LI   R5, 1
    CLR  *R13               ; caller R0 = 0
    RTWP
.Lnext_fib:
    MOV  R5, *R13           ; caller R0 = current fib
    MOV  R4, R0
    MOV  R5, R4
    A    R0, R5
    RTWP
