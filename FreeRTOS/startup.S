; ---------------------------------------------------------------------------
; TMS9900 FreeRTOS startup
;
; Memory map:
;   0x0000-0x003F  Interrupt vectors (16 levels × 2 words)
;   0x0040-0x005F  Tick ISR workspace (32 bytes)
;   0x0060-0x00FF  ISR stack (grows down from 0x0100)
;   0x0100-...     Code (.text)
;   ...-...        Data, BSS, task stacks
;   0xF000         Main stack top (startup/idle, grows down)
;
; The level 0 (reset) vector points here.
; The level 1 (tick) vector is written at runtime by xPortStartScheduler.
; ---------------------------------------------------------------------------

        .section .vectors, "a"

        ; Level 0 (RESET) vector
        .short  0x8300          ; WP = main workspace
        .short  _start          ; PC = entry point

        ; Level 1 (tick) - filled at runtime by port.c
        .short  0               ; placeholder WP
        .short  0               ; placeholder PC

        ; Levels 2-15 - unused, point to _unhandled_irq
        .short  0x0040, _unhandled_irq  ; Level 2
        .short  0x0040, _unhandled_irq  ; Level 3
        .short  0x0040, _unhandled_irq  ; Level 4
        .short  0x0040, _unhandled_irq  ; Level 5
        .short  0x0040, _unhandled_irq  ; Level 6
        .short  0x0040, _unhandled_irq  ; Level 7
        .short  0x0040, _unhandled_irq  ; Level 8
        .short  0x0040, _unhandled_irq  ; Level 9
        .short  0x0040, _unhandled_irq  ; Level 10
        .short  0x0040, _unhandled_irq  ; Level 11
        .short  0x0040, _unhandled_irq  ; Level 12
        .short  0x0040, _unhandled_irq  ; Level 13
        .short  0x0040, _unhandled_irq  ; Level 14
        .short  0x0040, _unhandled_irq  ; Level 15

; ---------------------------------------------------------------------------
        .section .startup, "ax"

        .global _start
_start:
        ; Mask interrupts before initializing C runtime state. The loader/reset
        ; vector supplies WP=0x8300; initialized .data is loaded directly into
        ; RAM by this flat-image format and needs no ROM-to-RAM copy.
        LIMI    0

        ; Set up main stack pointer (R10) for startup C code
        LI      R10, 0xF000

        ; Clear exactly the BSS bytes. MOVB auto-increment advances by one,
        ; handles odd sizes/addresses, and equality works across 0x8000.
        LI      R0, __bss_start
        LI      R1, __bss_end
        CLR     R2
.Lbss_loop:
        C       R0, R1
        JEQ     .Lbss_done
        MOVB    R2, *R0+
        JMP     .Lbss_loop
.Lbss_done:

        ; Call main()
        BL      @main

        ; If main returns, halt
        LIMI    0
.Lhalt:
        JMP     .Lhalt

; ---------------------------------------------------------------------------
; Unhandled interrupt - just return
; ---------------------------------------------------------------------------
        .global _unhandled_irq
_unhandled_irq:
        RTWP
