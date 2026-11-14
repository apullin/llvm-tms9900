; Exemplar 08: TI-99/4A Cartridge ROM Header (>6000), Safe CRT0, & PAD Bank-Switch Trampoline
; Shows standard 0xAA header, Pascal menu string + .p2align 1, .data/.bss init, and 378 banking.

    .section .cart_header, "a"
    .globl cart_header
cart_header:
    .byte  0xAA, 0x01       ; Standard TI-99/4A cartridge identifier + version
    .short 0x0000           ; Number of programs (unused)
    .short 0x0000           ; Power-up routine list (0 = none)
    .short prog_entry       ; Program list pointer
    .short 0x0000           ; DSR list
    .short 0x0000           ; Subprogram list

prog_entry:
    .short 0x0000           ; Next menu entry (0 = last)
    .short _start           ; Entry PC
    .byte  12               ; Title length
    .ascii "LLVM99 DEMO!"
    .p2align 1              ; Realign after odd-length byte+ascii table!

    .text
    .p2align 1
    .globl _start
    .globl install_pad_bank_trampoline

_start:
    LIMI 0                  ; Mask all interrupts immediately
    CLR  @0x6000            ; Deterministically latch ROM Bank 0 on 378-style banked carts
    LWPI 0x8300             ; Fast 16-bit 0-wait Scratchpad workspace
    LI   R10, 0xFFFC        ; 4-byte-aligned C stack top in upper 32K RAM

    ; Copy .data from ROM (__data_load_start) to RAM (__data_start..__data_end)
    LI   R0, __data_load_start
    LI   R1, __data_start
    LI   R2, __data_end
.Lcopy_data:
    C    R1, R2
    JEQ  .Lclear_bss
    MOVB *R0+, *R1+
    JMP  .Lcopy_data

.Lclear_bss:
    LI   R1, __bss_start
    LI   R2, __bss_end
    CLR  R0
.Lbss_loop:
    C    R1, R2
    JEQ  .Lenter_main
    MOVB R0, *R1+
    JMP  .Lbss_loop

.Lenter_main:
    BL   @main
.Lspin:
    JMP  .Lspin

; Copies a 14-byte far-call trampoline into safe Scratchpad PAD (0x8320..0x832D)
; so code in Bank 0 can call a function in Bank N (`R1 = 2*N`, `R2 = target_fn`) and return to Bank 0!
install_pad_bank_trampoline:
    LI   R0, .Ltramp_src
    LI   R1, 0x8320
    LI   R2, 7              ; 7 words = 14 bytes
.Ltramp_cp:
    MOV  *R0+, *R1+
    DEC  R2
    JNE  .Ltramp_cp
    B    *R11

.Ltramp_src:
    CLR  @0x6000(R1)        ; 4B: switch to target bank (R1 = 2*bank, R1 != R0!)
    MOV  R11, R9            ; 2B: save caller return PC (or push on *R10)
    BL   *R2                ; 2B: call target function in banked ROM
    CLR  @0x6000            ; 4B: restore Bank 0
    B    *R9                ; 2B: return to caller in Bank 0!
