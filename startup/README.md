# TMS9900 Startup Code

This directory contains startup code templates for bare-metal TMS9900 applications.

## Files

- `startup.asm` - Main startup template with vector table, reset handler, and interrupt handling

## Usage

1. Copy `startup.asm` to your project
2. Customize the memory addresses for your hardware:
   - `RESET_WS`, `IRQ1_WS` - Workspace addresses (must be in RAM)
   - `RAM_START` - Start of RAM for stack/BSS
   - Code origin (`AORG >0100`) - Where your code starts
3. Customize interrupt handlers as needed
4. Assemble and link with your C code

## How It Works

### TMS9900 Interrupt Mechanism

The TMS9900 has a unique interrupt handling mechanism based on workspace switching:

1. **Hardware detects interrupt** on INTREQ* pin with level on IC0-IC3
2. **CPU automatically performs `BLWP @vector`**:
   - Fetches new Workspace Pointer from vector address
   - Fetches new Program Counter from vector+2
   - Saves old WP→R13, PC→R14, ST→R15 in the NEW workspace
   - Sets interrupt mask to (level - 1) to allow higher priority interrupts
3. **Your ISR runs** with its own private register set (R0-R12 available)
4. **ISR ends with `RTWP`** which restores WP, PC, ST from R13-R15

### Vector Table Layout

The vector table occupies addresses 0x0000-0x003F:

| Level | Address | Purpose |
|-------|---------|---------|
| 0 | 0x0000 | RESET (cannot be disabled) |
| 1 | 0x0004 | External interrupt |
| 2 | 0x0008 | External interrupt |
| ... | ... | ... |
| 15 | 0x003C | External interrupt |

Each vector is 4 bytes: 2 bytes for Workspace Pointer, 2 bytes for Program Counter.

### Calling C from Interrupts

If your interrupt handler needs to call C functions:

1. Each ISR that calls C needs its own stack area
2. Set R10 to the stack top before calling C
3. C functions use R10 as stack pointer and follow the standard calling convention

Example:
```asm
IRQ1_Handler:
    LI   R10,IRQ_STACK_TOP    ; Set up stack
    BL   @my_c_handler        ; Call C function
    RTWP                      ; Return from interrupt
```

The C function can be a normal function:
```c
void my_c_handler(void) {
    // Handle interrupt
    // Can use local variables, call other functions, etc.
}
```

Or use the `interrupt` attribute for direct ISRs:
```c
void __attribute__((interrupt)) direct_isr(void) {
    // This function will use RTWP to return
    // But you still need to set up the vector table in asm
}
```

## Memory Considerations

### Workspaces

Each active context (main program, each interrupt level that can nest) needs its own workspace - 32 bytes of RAM for registers R0-R15.

### Stacks

- Main program needs a stack if using C
- Each interrupt handler that calls C needs its own stack
- Stack size depends on call depth and local variable usage

### Typical Memory Map

```
0x0000 - 0x003F : Interrupt vectors (64 bytes, hardware requirement)
0x0040 - 0x007F : XOP vectors (optional)
0x0080 - 0x00FF : Reserved/available
0x0100 - ...    : Code
...    - 0x82FF : More code/data
0x8300 - 0x831F : Reset workspace (32 bytes)
0x8320 - 0x833F : IRQ1 workspace (32 bytes)
0x8340 - 0x83BF : Stacks (adjust size as needed)
0x83C0 - ...    : Heap/BSS
```

Note: On TI-99/4A, 0x8300-0x83FF is the 256-byte "scratchpad RAM" which is faster than expansion RAM.

## Building

**Using LLVM toolchain directly (recommended):**

```bash
# Compile C code to object file
clang --target=tms9900 -c main.c -o main.o

# Assemble startup code to object file
clang --target=tms9900 -c startup.S -o startup.o

# Link with LLD (once TMS9900 LLD support is complete)
ld.lld -T ti99cart.ld startup.o main.o -o program.elf

# Extract binary
llvm-objcopy -O binary program.elf program.bin
```

**Using xas99 (legacy workflow):**

```bash
# Assemble startup code
xas99.py -R startup.asm -o startup.o

# Compile C code with LLVM and convert to xas99 format
clang --target=tms9900 -S main.c -o main.s
python3 llvm2xas99.py main.s > main.asm
xas99.py -R main.asm -o main.o

# Link with your linker of choice
```

## See Also

- [PROJECT_JOURNAL.md](../PROJECT_JOURNAL.md) - Full backend documentation
- [TMS 9900 Data Manual](../TMS_9900_Microprocessor_Data_Manual_May76.pdf) - Hardware reference
