; ---------------------------------------------------------------------------
; FreeRTOS TMS9900 port - context switch assembly
;
; The TMS9900 workspace pointer architecture makes context switching
; trivially cheap: each task owns a permanent 32-byte workspace in RAM
; (its "register file"), and switching tasks means changing the WP register.
;
; Context frame layout on the task's data stack (4 words):
;   [SP+0] = WP   (workspace pointer)
;   [SP+2] = PC   (resume address)
;   [SP+4] = ST   (status register)
;   [SP+6] = usCriticalNesting
;
; pxTopOfStack in the TCB points to [WP] (lowest address).
;
; ---------------------------------------------------------------------------

        .text

; ISR workspace address (must match port.c portISR_WP_ADDR)
        .equ ISR_WP, 0x0040

; External symbols from FreeRTOS kernel
        .extern pxCurrentTCB
        .extern xTaskIncrementTick
        .extern vTaskSwitchContext
        .extern usCriticalNesting

; ---------------------------------------------------------------------------
; vPortStartFirstTask
;
; Called from xPortStartScheduler.  Switches to the ISR workspace and
; restores the first task's context from pxCurrentTCB.
; Never returns.
; ---------------------------------------------------------------------------
        .global vPortStartFirstTask
vPortStartFirstTask:
        LWPI    ISR_WP

        ; -- RESTORE CONTEXT --
        ; Load pxTopOfStack from current TCB
        MOV     @pxCurrentTCB, R1
        MOV     *R1, R0

        ; Pop context frame
        MOV     *R0+, R13               ; WP
        MOV     *R0+, R14               ; PC
        MOV     *R0+, R15               ; ST
        MOV     *R0+, R2                ; usCriticalNesting
        MOV     R2, @usCriticalNesting

        ; Set R10 in the target workspace to updated data stack pointer
        MOV     R0, @20(R13)

        ; Enter task
        RTWP

; ---------------------------------------------------------------------------
; vPortTickISR
;
; Entered via level 1 interrupt vector.
; The hardware has already:
;   - Loaded ISR workspace (WP = 0x0040)
;   - Saved old WP -> R13, old PC -> R14, old ST -> R15
;   - Set interrupt mask to level-1 = 0 (all maskable IRQs blocked)
; ---------------------------------------------------------------------------
        .global vPortTickISR
vPortTickISR:
        ; -- SAVE CONTEXT --
        ; Get interrupted task's data stack pointer (R10 in old workspace)
        MOV     @20(R13), R0

        ; Push context frame onto task's data stack
        DECT    R0
        MOV     @usCriticalNesting, *R0 ; push critNesting
        DECT    R0
        MOV     R15, *R0                ; push ST
        DECT    R0
        MOV     R14, *R0                ; push PC
        DECT    R0
        MOV     R13, *R0                ; push WP

        ; Update task's R10 to new stack pointer
        MOV     R0, @20(R13)

        ; Store pxTopOfStack into TCB
        MOV     @pxCurrentTCB, R1
        MOV     R0, *R1

        ; -- TICK + SCHEDULE --
        BL      @xTaskIncrementTick
        MOV     R0, R0                  ; test return value
        JEQ     .Ltick_no_switch
        BL      @vTaskSwitchContext
.Ltick_no_switch:

        ; -- RESTORE CONTEXT --
        MOV     @pxCurrentTCB, R1
        MOV     *R1, R0

        MOV     *R0+, R13               ; WP
        MOV     *R0+, R14               ; PC
        MOV     *R0+, R15               ; ST
        MOV     *R0+, R2                ; usCriticalNesting
        MOV     R2, @usCriticalNesting

        MOV     R0, @20(R13)

        RTWP

; ---------------------------------------------------------------------------
; vPortYield
;
; Voluntary context switch, called via BL from task code.
; The task's own workspace is active; R11 = return address.
;
; Strategy: save context frame on task's stack, switch to ISR workspace
; (LWPI), call vTaskSwitchContext, restore new task via RTWP.
; ---------------------------------------------------------------------------
        .global vPortYield
vPortYield:
        ; Capture status register before disabling interrupts
        STST    R0
        LIMI    0

        ; Get our own workspace pointer
        STWP    R2

        ; Push context frame onto our data stack (R10)
        DECT    R10
        MOV     @usCriticalNesting, *R10 ; push critNesting
        DECT    R10
        MOV     R0, *R10                ; push ST (captured above)
        DECT    R10
        MOV     R11, *R10               ; push PC (return address from BL)
        DECT    R10
        MOV     R2, *R10                ; push WP

        ; Store pxTopOfStack into TCB
        MOV     @pxCurrentTCB, R1
        MOV     R10, *R1

        ; Switch to ISR workspace for the context switch
        ; (so we don't corrupt any task's R13-R15)
        LWPI    ISR_WP

        ; Call scheduler to pick next task
        BL      @vTaskSwitchContext

        ; -- RESTORE CONTEXT (same as tick ISR) --
        MOV     @pxCurrentTCB, R1
        MOV     *R1, R0

        MOV     *R0+, R13               ; WP
        MOV     *R0+, R14               ; PC
        MOV     *R0+, R15               ; ST
        MOV     *R0+, R2                ; usCriticalNesting
        MOV     R2, @usCriticalNesting

        MOV     R0, @20(R13)

        RTWP
