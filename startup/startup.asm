* ============================================================
* TMS9900 Bare Metal Startup Template
* ============================================================
*
* This file provides a starting point for bare-metal TMS9900
* applications. Customize the memory addresses and handlers
* for your specific hardware.
*
* Memory Layout (customize for your system):
*   0x0000-0x003F: Interrupt vector table (hardware requirement)
*   0x0040-0x007F: XOP trap vectors (optional)
*   WORKSPACES:    RAM area for register workspaces
*   STACK:         RAM area for C runtime stack
*   CODE:          Your program code
*
* ============================================================

* ============================================================
* Configuration - CUSTOMIZE THESE FOR YOUR HARDWARE
* ============================================================

* Workspace addresses - each workspace needs 32 bytes (16 words)
* These should be in fast RAM if available
RESET_WS   EQU  >8300        ; Workspace for Reset/main program
IRQ1_WS    EQU  >8320        ; Workspace for Level 1 interrupts

* Stack configuration
STACK_SIZE EQU  256          ; Main stack size in bytes
IRQ_STACK_SIZE EQU 64        ; IRQ stack size in bytes

* RAM start for BSS (adjust based on your system)
RAM_START  EQU  >8340

* ============================================================
* Vector Table - MUST be at address 0x0000
* ============================================================

       AORG >0000

* Level 0: RESET (hardware requirement - cannot be disabled)
       DATA RESET_WS          ; Workspace pointer for reset
       DATA Reset_Handler     ; Entry point after reset

* Level 1: External interrupt (directly usable)
       DATA IRQ1_WS           ; Workspace pointer for IRQ1
       DATA IRQ1_Handler      ; IRQ1 entry point

* Levels 2-15: Default handlers (reuse RESET workspace since they just spin)
* Customize these if you need to handle specific interrupt levels
       DATA RESET_WS, Default_Handler    ; Level 2
       DATA RESET_WS, Default_Handler    ; Level 3
       DATA RESET_WS, Default_Handler    ; Level 4
       DATA RESET_WS, Default_Handler    ; Level 5
       DATA RESET_WS, Default_Handler    ; Level 6
       DATA RESET_WS, Default_Handler    ; Level 7
       DATA RESET_WS, Default_Handler    ; Level 8
       DATA RESET_WS, Default_Handler    ; Level 9
       DATA RESET_WS, Default_Handler    ; Level 10
       DATA RESET_WS, Default_Handler    ; Level 11
       DATA RESET_WS, Default_Handler    ; Level 12
       DATA RESET_WS, Default_Handler    ; Level 13
       DATA RESET_WS, Default_Handler    ; Level 14
       DATA RESET_WS, Default_Handler    ; Level 15

* ============================================================
* XOP Vectors (0x0040-0x007F) - Optional
* ============================================================
* Uncomment and customize if using XOP instructions
*      AORG >0040
*      DATA XOP0_WS, XOP0_Handler    ; XOP 0
*      DATA XOP1_WS, XOP1_Handler    ; XOP 1
*      ... etc for XOP 2-15 ...

* ============================================================
* Code Section
* ============================================================

       AORG >0100            ; Start code after vectors (adjust as needed)

* ------------------------------------------------------------
* Reset_Handler - Called at power-on/reset
* ------------------------------------------------------------
* The CPU has already:
*   - Loaded RESET_WS into the Workspace Pointer
*   - Jumped here
*   - Saved (garbage) old context to R13-R15
*
* We need to:
*   1. Disable interrupts during initialization
*   2. Set up the stack pointer (R10) for C code
*   3. Optionally initialize .bss and .data sections
*   4. Call main()
*   5. Hang if main() returns
* ------------------------------------------------------------
Reset_Handler:
       LIMI 0                 ; Disable interrupts during init

       * Set up stack pointer for C runtime
       * Stack grows downward, so point to top of stack area
       LI   R10,STACK_TOP

       * Optional: Zero .bss section
       * BL   @_zero_bss

       * Optional: Copy .data section from ROM to RAM
       * BL   @_copy_data

       * Call C main function
       BL   @main

       * If main() returns, hang here
       * Could also do a soft reset: B @Reset_Handler
_hang:
       JMP  _hang

* ------------------------------------------------------------
* IRQ1_Handler - Level 1 Interrupt Handler
* ------------------------------------------------------------
* The CPU has already:
*   - Loaded IRQ1_WS into the Workspace Pointer
*   - Saved old WP to R13, old PC to R14, old ST to R15
*   - Jumped here
*   - Set interrupt mask to 0 (disabling lower priority interrupts)
*
* The handler has a fresh set of registers R0-R12 to use.
* R13-R15 hold the return context - DO NOT MODIFY these.
*
* If calling C code, set up R10 as stack pointer first.
* ------------------------------------------------------------
IRQ1_Handler:
       * OPTION A: Simple handler that doesn't call C
       * Just do your work using R0-R12 and return
       NOP                    ; Replace with your ISR code
       RTWP                   ; Return from interrupt

       * OPTION B: Handler that calls C code
       * Uncomment this instead of the above:
*      LI   R10,IRQ_STACK_TOP  ; Set up stack for C
*      BL   @irq1_handler_c    ; Call C handler
*      RTWP                    ; Return from interrupt

* ------------------------------------------------------------
* Default_Handler - Catches undefined interrupts
* ------------------------------------------------------------
* This handler is used for interrupt levels that aren't
* explicitly handled. It reuses the RESET workspace since
* it just spins forever (or you could reset).
* ------------------------------------------------------------
Default_Handler:
       * Option 1: Infinite loop (for debugging - attach debugger here)
       JMP  Default_Handler

       * Option 2: Reset the system
       * B    @Reset_Handler

* ============================================================
* Stack and Workspace Areas
* ============================================================

       AORG RAM_START

* Main program stack (grows downward)
STACK_BOTTOM BSS STACK_SIZE
STACK_TOP    EQU $

* IRQ stack (if calling C from interrupt handlers)
IRQ_STACK_BOTTOM BSS IRQ_STACK_SIZE
IRQ_STACK_TOP    EQU $

* ============================================================
* External References
* ============================================================

       REF  main              ; C main function

* Optional: C interrupt handler
*      REF  irq1_handler_c

* ============================================================
       END  Reset_Handler
