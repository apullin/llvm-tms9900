# LLVM-99: LLVM Backend for the TMS9900 Microprocessor

## Project Overview

This project implements an LLVM backend for the Texas Instruments TMS9900 microprocessor, the CPU used in the TI-99/4A home computer (1981). The TMS9900 was the first single-chip 16-bit microprocessor and has several unique architectural features that required special handling in the compiler.

### Target Architecture

- **Word size**: 16-bit
- **Endianness**: Big-endian
- **Registers**: 16 general-purpose 16-bit registers (R0-R15)
- **Unique feature**: Registers are memory-mapped via the Workspace Pointer (WP)
- **No hardware stack**: Software stack implemented using R10 as stack pointer

## Implemented Features

### Core Functionality

#### 16-bit Integer Operations
All standard 16-bit operations are supported natively using TMS9900 instructions:
- **Arithmetic**: ADD (`A`), SUB (`S`), NEG, ABS, INC, DEC, INCT, DECT
- **Logic**: OR (`SOC`), XOR, NOT (`INV`)
- **Shifts**: SLA, SRA, SRL, SRC (rotate right)
- **Compare**: C (sets status bits for signed/unsigned comparisons)

#### The AND Instruction Problem
**TMS9900 Quirk**: The TMS9900 has no AND instruction! Instead, it has:
- `SZC` (Set Zeros Corresponding): `dst = dst AND (NOT src)` (bit clear)
- `SOC` (Set Ones Corresponding): `dst = dst OR src`

**Solution**: We implement AND using a three-instruction pseudo-expansion:
```asm
; To compute: rd = rs1 AND rs2
INV  rs2        ; rs2 = NOT rs2
SZC  rs2, rd    ; rd = rd AND (NOT rs2) = rd AND original_rs2
INV  rs2        ; Restore rs2 to original value
```
This is handled in `TMS9900InstrInfo.cpp:expandPostRAPseudo()`.

#### 8-bit (Byte) Operations
The TMS9900 has byte instructions (MOVB, AB, SB, etc.) that operate on the **upper byte** of registers. Rather than dealing with this complexity throughout the backend, we promote all i8 operations to i16. Byte loads/stores use MOVB which handles the byte positioning automatically.

#### 32-bit Integer Operations
The TMS9900 is a 16-bit CPU, so 32-bit operations require special handling:

- **Addition/Subtraction**: LLVM automatically expands these to pairs of 16-bit operations with carry handling. We synthesize carry propagation since TMS9900 lacks ADC/SBC instructions.

- **Multiplication**: LLVM expands to partial products using 16-bit multiplies.

- **Division/Remainder**: Uses libcalls (`__divsi3`, `__udivsi3`, `__modsi3`, `__umodsi3`) from the runtime library because inline expansion would be too complex.

- **Shifts**: Variable-amount shifts use libcalls (`__ashlsi3`, `__ashrsi3`, `__lshrsi3`) from the runtime library. Constant shifts are optimized by LLVM at compile time. The runtime implementation uses a word-swap optimization for shifts ≥16 bits, then loops for the remaining 0-15 bits. Edge cases handled: shift by 0 (return unchanged), shift ≥32 (return 0 or sign-extended -1 for arithmetic right shift).

#### 16-bit Multiply and Divide
The TMS9900 has native multiply and divide instructions with quirks:

**MPY (Multiply)**: 16x16 -> 32-bit result
- Produces 32-bit result in register pair Rd:Rd+1
- Rd must be an even register
- We hardcode to R0 and extract the low 16 bits from R1 for normal `mul`

**DIV (Divide)**: 32-bit / 16-bit -> 16-bit quotient + 16-bit remainder
- Dividend must be in Rd:Rd+1 (even register pair)
- For 16-bit divide, we clear the high word and place dividend in low word
- Quotient ends up in Rd, remainder in Rd+1

**Signed Division**: The DIV instruction is unsigned-only. For signed division, we:
1. Save the XOR of operand signs (determines result sign)
2. Take absolute values of both operands
3. Perform unsigned division
4. Negate result if signs differed

This is implemented via pseudo-instructions (`SDIV16`, `UDIV16`, `SREM16`, `UREM16`) that expand in `EmitInstrWithCustomInserter`.

### Calling Convention

Defined in `TMS9900CallingConv.td`:

| Register | Purpose |
|----------|---------|
| R0 | Return value |
| R1-R9 | Arguments (first 9 words), caller-saved |
| R10 | Stack Pointer (SP) |
| R11 | Link Register (return address) |
| R12 | Scratch (caller-saved) |
| R13-R15 | Callee-saved |

- **32-bit values** use register pairs: R0:R1 (high:low) for return, R1:R2, R3:R4 for args
- **Additional arguments** spill to the stack
- **Stack grows downward** (high to low addresses)

### Stack Frame and Spilling

**No Hardware Stack**: TMS9900 has no PUSH/POP or hardware stack pointer. We implement a software stack:
- R10 serves as the stack pointer
- Push: `DECT R10` then `MOV Rx,*R10`
- Pop: `MOV *R10+,Rx`

**Indexed Stack Access**: For register spills and local variables, we use indexed addressing:
```asm
MOV  R5,@4(R10)   ; Store R5 at SP+4
MOV  @6(R10),R6   ; Load from SP+6 into R6
```
This is implemented via `MOV_FI_Load` and `MOV_FI_Store` pseudo-instructions that get frame indices resolved in `eliminateFrameIndex`.

### Signed vs Unsigned Comparisons

The TMS9900 compare instruction (`C Rs,Rd`) sets multiple status bits:
- **ST0 (L>)**: Logical (unsigned) greater than
- **ST1 (A>)**: Arithmetic (signed) greater than
- **ST2 (EQ)**: Equal

Different jump instructions test different conditions:
- **JGT**: ST1=1 (signed greater than)
- **JLT**: ST1=0 AND ST2=0 (signed less than)
- **JHE**: ST0=1 OR ST2=1 (unsigned high or equal)
- **JLE**: ST0=0 OR ST2=1 (unsigned low or equal)
- **JEQ/JNE**: ST2=1/0 (equal/not equal)

We map LLVM's comparison conditions to appropriate jumps, sometimes swapping operands to simplify:
- `a >= b` becomes: `C b,a` + `JGT false_branch` (using !(b > a))
- `a <= b` becomes: `C a,b` + `JGT false_branch` (using !(a > b))

### Switch Statements / Jump Tables

Switch statements compile to jump tables for efficiency:
```asm
SLA  R0,1          ; Index * 2 (word offset)
MOV  @LJTI0(R0),R1 ; Load target address from table
B    *R1           ; Indirect branch

LJTI0:
    DATA L_case0
    DATA L_case1
    DATA L_case2
```
Implemented via `BR_JT` expansion and custom `JumpTable` lowering.

### Auto-Increment Addressing

TMS9900 supports post-increment addressing (`*R+`):
- Word operations increment by 2
- Byte operations increment by 1

We expose this to LLVM via `setIndexedLoadAction(ISD::POST_INC, ...)` allowing the optimizer to combine pointer arithmetic with loads/stores:
```asm
MOV  *R3+,R5    ; Load word at R3, then R3 += 2
```

### Inline Assembly

Full support for inline assembly with:
- **Register constraints**: `r` (any register), `{R0}` through `{R15}` (specific registers)
- **Immediate constraints**: `i`, `n`
- **Memory constraints**: `m`

Example:
```c
int result;
asm("MPY %1,R0\nMOV R1,%0" : "=r"(result) : "r"(multiplier) : "R0", "R1");
```

### Varargs Support

Variable argument functions are supported:
- `va_start` stores the frame pointer to the va_list location
- `va_arg` loads successive arguments from the stack
- Arguments beyond R0-R3 are already on the stack from the caller

### Atomic Operations

**TMS9900 Quirk**: The TMS9900 has no atomic instructions, cache, or memory barriers. It's a simple single-core CPU where all memory operations are inherently ordered and immediately visible.

**Solution**: We tell LLVM to expand all atomic operations to regular loads/stores via `shouldExpandAtomicLoadInIR()` returning true. This is correct because there's no concurrency that could observe non-atomic behavior.

### Instruction Scheduling

Defined in `TMS9900Schedule.td` based on timing data from the TMS 9900 Data Manual (Table 3):

| Operation | Base Cycles | Notes |
|-----------|-------------|-------|
| Fast (B, CLR, INC, INV) | 8-10 | |
| ALU (MOV, A, S, C) | 14 | Register-to-register |
| Memory access | +4-8 | Addressing mode overhead |
| MPY | 52 | |
| DIV | 92-124 | Data dependent |
| Shifts | 12 + 2*count | |

The scheduler uses this to prefer shorter instructions when order doesn't matter.

### Byte Swap

The TMS9900 has `SWPB` which swaps the two bytes of a register, directly supporting `__builtin_bswap16()`.

---

## xas99 Assembly Translator

### Why a Translator is Needed

The LLVM backend generates assembly in a format based on LLVM's MC layer conventions:
- Section directives like `.text`, `.data`, `.bss`
- Alignment directives like `.p2align`
- ELF-style directives like `.type`, `.size`

However, the xas99 assembler (from the xdt99 toolkit) uses traditional TI assembler syntax:
- `DEF`, `REF`, `EVEN`, `BSS`, `DATA`, `TEXT`, `BYTE`, `END`

### The Translator Script

`llvm2xas99.py` post-processes LLVM output for xas99 compatibility:

```bash
# Usage:
llc -march=tms9900 input.ll -o - | python3 llvm2xas99.py > output.asm

# Or:
llc -march=tms9900 input.ll -o temp.s
python3 llvm2xas99.py temp.s > output.asm
```

### Transformations Applied

| LLVM Output | xas99 Output |
|-------------|--------------|
| `.text` | `; === TEXT SECTION ===` |
| `.data` | `; === DATA SECTION ===` |
| `.bss` | `; === BSS SECTION ===` |
| `.p2align N` | `EVEN` |
| `.zero N` | `DATA 0` (repeated) |
| `.ascii "str"` | `TEXT 'str'` |
| `.asciz "str"` | `TEXT 'str'` + `BYTE 0` |
| `.type`, `.size`, etc. | (removed) |

### External Reference Detection

The translator automatically detects references to external symbols (like libcall functions) and generates `REF` directives:
```asm
; External references (libcalls)
    REF __mulsi3
    REF __divsi3
```

### Native xas99 Dialect (Update)

We added native xas99 dialect support directly to the LLVM backend:

```bash
clang --target=tms9900 -O2 -S -fno-addrsig \
      -mllvm -tms9900-asm-dialect=xas99 test.c -o test.s
```

**Features**:
- Hex immediates as `>XXXX` format (not `0x...`)
- Negative values as two's complement (e.g., `-1` → `>FFFF`)
- `DEF` for exported symbols
- `BSS N` for zero-fill (not `.zero N`)

**Quirk**: LLVM's MC layer always emits certain directives (`.text`, `.data`, `.bss`, `.p2align`) when switching sections. These cannot be suppressed without a custom MCStreamer. Workaround:

```bash
grep -v '^\s*\.' test.s > test_clean.s
```

For complex projects, `llvm2xas99.py` still provides more comprehensive conversion

---

## Runtime Library

Located in `runtime/tms9900_rt.asm`, this provides compiler support functions:

### Functions

| Function | Purpose | Status |
|----------|---------|--------|
| `__mulsi3` | 32-bit multiply | Implemented |
| `__divsi3` | 32-bit signed divide | Implemented |
| `__udivsi3` | 32-bit unsigned divide | Implemented |
| `__modsi3` | 32-bit signed remainder | Implemented |
| `__umodsi3` | 32-bit unsigned remainder | Implemented |
| `__ashlsi3` | 32-bit left shift | Implemented |
| `__ashrsi3` | 32-bit arithmetic right shift | Implemented |
| `__lshrsi3` | 32-bit logical right shift | Implemented |

### Building

```bash
# Assemble standalone object
xas99.py -R runtime/tms9900_rt.asm -o tms9900_rt.o

# Or include directly in your program
cat your_code.asm runtime/tms9900_rt.asm > combined.asm
xas99.py -R combined.asm -b -o program.bin
```

---

## Deferred Features

### Conditional Branch Analysis
**What**: LLVM's branch analysis allows dead code elimination and branch optimization.
**Status**: Partially implemented (unconditional branches only).
**Why Deferred**: Low optimization impact. Dead code elimination happens at IR level anyway. The main benefit would be slightly cleaner assembly output.

### Floating Point
**What**: Software floating point emulation.
**Status**: Not implemented.
**Why Deferred**: Would require a substantial softfloat library. The TI-99/4A rarely used floating point in assembly programs. Could be added by implementing `__addsf3`, `__mulsf3`, etc.

### Frame Pointer
**What**: Dedicated frame pointer register for debugging/unwinding.
**Status**: Not used (hasFP returns false).
**Why Deferred**: Not needed for correctness. Could be added for debugger support.

---

## Interrupts and Bare-Metal Support

### TMS9900 Interrupt Mechanism

The TMS9900 has a unique interrupt architecture based on workspace switching:

1. **16 Priority Levels**: Level 0 (RESET, highest) through Level 15 (lowest)
2. **Vector Table** at 0x0000-0x003F: Each level has a 4-byte vector (WP, PC)
3. **Automatic Context Switch**: On interrupt, the CPU:
   - Fetches new Workspace Pointer from vector
   - Fetches new Program Counter from vector+2
   - Saves old WP→R13, PC→R14, ST→R15 in the NEW workspace
   - Sets interrupt mask to (level - 1)
4. **Return via RTWP**: Restores WP, PC, ST from R13-R15

This means each interrupt handler gets a **fresh set of registers** (R0-R12) without any software save/restore overhead.

### Function Attributes

#### `naked` Attribute

Generates no prologue or epilogue. The user is responsible for the entire function body including return:

```c
void __attribute__((naked)) my_asm_func(void) {
    asm volatile(
        "LI R0, 42\n"
        "B *R11"        // User provides return
    );
}
```

Output:
```asm
my_asm_func:
    LI R0, 42
    B *R11
```

#### `interrupt` Attribute

Generates no prologue/epilogue and returns with RTWP instead of B *R11:

```c
void __attribute__((interrupt)) my_isr(void) {
    // ISR code - can use R0-R12 freely
    // R13-R15 hold return context, don't modify
}
```

Output:
```asm
my_isr:
    ; ... your code ...
    RTWP                ; Return from interrupt
```

**Note**: The `interrupt` attribute only affects code generation. You still need to:
1. Set up the vector table (in assembly or linker script)
2. Allocate workspace memory for the ISR
3. Set up a stack (R10) if calling C functions from the ISR

### Startup Code Templates

The `startup/` directory contains templates for bare-metal applications:

- `startup.asm` - Vector table, reset handler, interrupt handlers
- `README.md` - Detailed usage instructions

See [startup/README.md](startup/README.md) for complete documentation.

### Calling C from Interrupt Handlers

If your ISR needs to call C functions:

```asm
IRQ1_Handler:
    LI   R10,IRQ_STACK_TOP    ; Set up stack for C calling convention
    BL   @my_c_handler        ; Call C function
    RTWP                      ; Return from interrupt
```

The C handler is a normal function:
```c
void my_c_handler(void) {
    // Handle interrupt
    // Uses normal calling convention (R10=SP, R11=LR)
}
```

---

## Not Yet Implemented

### Workspace Pointer Context Switching (BLWP Calling Convention)

**What**: The TMS9900's `BLWP` instruction can call a subroutine while atomically switching to a new register set, and `RTWP` returns while restoring the old workspace.

**Use Cases**:
- Coroutine-style context switching
- Multiple "threads" with separate register banks
- Fast subroutine calls without register save/restore

**Current Status**: The hardware mechanism works for interrupts (automatic BLWP). For explicit BLWP-style calls, use inline assembly.

**Potential Future Implementation**:
- New calling convention attribute: `__attribute__((workspace(0x8300)))`
- Generate `BLWP @vector` instead of `BL @function`

### CRU Bit I/O

**What**: TMS9900 has special instructions for bit-addressable I/O:
- `LDCR` - Load Communication Register (input bits)
- `STCR` - Store Communication Register (output bits)
- `SBO` - Set Bit to One
- `SBZ` - Set Bit to Zero
- `TB` - Test Bit

**Use Cases**: Keyboard scanning, cassette I/O, RS-232, speech synthesizer.

**Potential Implementation**: Intrinsic functions or inline assembly patterns.

### XOP (Extended Operations)

**What**: Software-implemented "extended instructions" via `XOP` trap.

**Use Cases**: OS services, debugger breakpoints.

**Status**: The instruction is defined but not used.

### Status Register Access

**What**: Direct manipulation of status register bits.
- `STST` - Store Status (save SR to register)
- `LST` - Load Status (unsupported on TMS9900, available on TMS9995)

**Use Cases**: Saving/restoring interrupt state, checking overflow flag.

**Potential Implementation**: Intrinsics like `__builtin_tms9900_stst()`.

### Memory-Mapped Register Access

**What**: Since TMS9900 registers are memory-mapped, you can access another context's registers directly.

**Example**: If WP=0x8300, then R0 is at 0x8300, R1 at 0x8302, etc.

**Use Cases**: Debuggers, context inspection, inter-context communication.

**Potential Implementation**: Would need careful interaction with register allocator.

---

## Building the Backend

```bash
cd llvm-project
mkdir build && cd build
cmake -G Ninja -DLLVM_TARGETS_TO_BUILD="TMS9900" \
      -DCMAKE_BUILD_TYPE=Release ../llvm
ninja
```

### Testing

```bash
# Compile LLVM IR to TMS9900 assembly
./bin/llc -march=tms9900 test.ll -o test.s

# Convert to xas99 format
python3 ../llvm2xas99.py test.s > test_xas.asm

# Assemble with xas99
xas99.py -R -o test.o test_xas.asm
```

### Execution Testing with tms9900-trace

The `tms9900-trace` tool provides a standalone TMS9900 CPU simulator for testing compiled code without requiring TI-99/4A ROMs or a full system emulator.

**Repository**: `~/personal/ti99/tms9900-trace/`

**Features**:
- Flat 64K RAM memory model (no ROM requirements)
- NDJSON trace output with PC, WP, ST, and all registers after each instruction
- Configurable load address, entry point, and workspace pointer
- Interrupt injection for ISR testing (`--irq=LEVEL@STEP`)
- Infinite loop detection for automatic termination
- Cycle-accurate timing based on TMS9900 Data Manual

**Complete Test Workflow**:

```bash
# 1. Compile LLVM IR to assembly
./bin/llc -march=tms9900 test.ll -o test.s

# 2. Convert to xas99 format
python3 ../llvm2xas99.py test.s > test.asm

# 3. Create a harness with startup code (see startup/startup.asm template)
#    Or assemble standalone with absolute addresses:
python3 ~/personal/ti99/xdt99/xas99.py -R test.asm -b -o test.bin

# 4. Run through trace simulator
~/personal/ti99/tms9900-trace/build/tms9900-trace \
    -l 0x0000 -e 0x0000 -w 0x8300 -n 1000 test.bin
```

**Example Output**:
```json
{"step":0,"pc":"0000","wp":"8300","st":"0000","clk":0,"op":"LWPI","asm":"LWPI >8300","r":["0000",...]}
{"step":1,"pc":"0004","wp":"8300","st":"0000","clk":44,"op":"LI","asm":"LI   R0,>000F","r":["000F",...]}
```

**Testing Interrupt Handlers**:
```bash
# Trigger IRQ level 1 after 500 instructions
./tms9900-trace --irq=1@500 -s 0x0200 test.bin
```

### Minimum Working Example

The `tests/` directory contains an end-to-end test that validates the complete toolchain:

```bash
./tests/run_mwe_test.sh
```

**Test Coverage** (`tests/mwe_test.c`):
- Array operations with pointer arithmetic
- Dot product computation (exercises MPY instruction)
- Bubble sort (exercises comparisons, swaps, nested loops)
- Global variable access
- Function calls

**Expected Results**:
- `result_dot1` = 55 (0x0037) - dot product before sort
- `result_dot2` = 86 (0x0056) - dot product after sort
- `sorted_array` = {1, 2, 5, 8, 9}
- Return value in R0 = 141 (0x008D)

---

## Direct Machine Code Emission (January 2026)

### Overview

We implemented direct ELF object file emission, eliminating the need for an external assembler (xas99) in many workflows. The compiler can now produce machine code bytes directly.

### Implementation

Created the MCTargetDesc layer with the following components:

| File | Purpose |
|------|---------|
| `TMS9900FixupKinds.h` | Defines relocation types (16-bit absolute, 8-bit PC-relative, 16-bit PC-relative) |
| `TMS9900MCCodeEmitter.cpp` | Encodes MCInst to binary bytes, big-endian |
| `TMS9900AsmBackend.cpp` | Handles fixup application, creates ELF writer |
| `TMS9900ELFObjectWriter.cpp` | Maps fixups to ELF relocation types |
| `TMS9900MCTargetDesc.cpp` | Registers all MC components |

### Instruction Format Variants

The TableGen format classes needed variants to handle different operand patterns for the same encoding:

**Two-Address Operations** (A, S, SOC, SZC, XOR):
- Input: `(outs $rd), (ins $rs1, $rs2)` with `$rd = $rs1` constraint
- Need to encode `$rd` and `$rs2` (not `$rs1` which is tied)
- Created: `Format1_Reg_TwoAddr`, `Format2_Reg_TwoAddr`

**Compare Operations** (C, CI):
- No output operand, just sets flags
- Created: `Format1_Reg_Cmp`, `Format8_Cmp`

**Shift Operations** (SLA, SRA, SRL, SRC):
- Two-address with count operand
- Created: `Format7_TwoAddr`, `Format7_R0Count`

**Implicit Register Operations** (MPY, DIV, RET):
- Some operands are hardcoded (R0 for MPY/DIV, R11 for RET)
- Created: `Format2_Reg_R0`, `Format3_Reg_R11`

### Usage

```bash
# Compile C to ELF object file
clang --target=tms9900 -c source.c -o source.o

# Examine the object file
llvm-readelf -a source.o

# Extract raw binary (for ROM/RAM loading)
llvm-objcopy -O binary source.o source.bin

# Or Intel HEX format
llvm-objcopy -O ihex source.o source.hex
```

### ELF Details

- **Class**: ELF32
- **Endianness**: Big-endian (MSB)
- **Machine**: None (EM_NONE = 0) - no official ELF machine type for TMS9900
- **OS/ABI**: Standalone (embedded)

### Workflow Comparison

**Old Workflow** (still supported):
```
C code → clang -S → assembly → llvm2xas99.py → xas99.py → binary
```

**New Workflow**:
```
C code → clang -c → ELF object → objcopy → raw binary
```

The new workflow is simpler and doesn't require external Python scripts or xas99.

### Limitations

- No disassembler yet (llvm-objdump can't disassemble TMS9900)
- No linker support (use objcopy for single-file programs, or external linker)
- No debug info emission

### Verification

The object file generation was verified by:
1. Compiling a simple function to .o file
2. Examining ELF structure with llvm-readelf
3. Extracting .text section with objcopy
4. Inspecting machine code bytes with xxd

---

## File Reference

### TableGen Files (`.td`)

| File | Purpose |
|------|---------|
| `TMS9900.td` | Main target description, processor features |
| `TMS9900RegisterInfo.td` | Register definitions (R0-R15, WP, ST) |
| `TMS9900CallingConv.td` | Calling convention (argument passing, callee-saved) |
| `TMS9900InstrInfo.td` | Instruction definitions and patterns |
| `TMS9900InstrFormats.td` | Instruction encoding formats |
| `TMS9900Schedule.td` | Instruction timing/scheduling model |

### C++ Implementation Files

| File | Purpose |
|------|---------|
| `TMS9900TargetMachine.cpp` | Target machine setup |
| `TMS9900Subtarget.cpp` | Subtarget features |
| `TMS9900ISelLowering.cpp` | DAG lowering (custom operation handling) |
| `TMS9900ISelDAGToDAG.cpp` | Instruction selection |
| `TMS9900InstrInfo.cpp` | Instruction info, pseudo expansion |
| `TMS9900RegisterInfo.cpp` | Register info, frame index elimination |
| `TMS9900FrameLowering.cpp` | Prologue/epilogue generation |
| `TMS9900AsmPrinter.cpp` | Assembly output |
| `TMS9900MCInstLower.cpp` | MC layer interface |

---

## References

- **TMS 9900 Microprocessor Data Manual** (May 1976) - Instruction set, timing
- **TI-99/4A Technical Data** - Memory map, I/O addresses
- **xdt99 Cross-Development Tools** - xas99 assembler documentation
- **LLVM Backend Tutorial** - General LLVM backend development

---

## Development Log

### 2026-01-04 DONE xas99-style labels without colons in AsmParser

**What**: Implemented support for xas99-style labels that don't require colons (e.g., `START LI R0,>1234` instead of `START: LI R0,>1234`)

**Where**: `llvm/lib/Target/TMS9900/AsmParser/TMS9900AsmParser.cpp` - added `isKnownMnemonic()`, `isKnownDirective()`, modified `ParseInstruction()`

**Why**: xas99 and traditional TI assembler syntax uses labels without colons. This enables direct assembly of existing TI-99/4A code and interoperability with the xas99 toolchain.

**Technical notes**:
- Added StringSaver member to properly manage string lifetime when lowercasing mnemonics
- Labels are uppercased for case-insensitive symbol matching (xas99 convention)
- When identifier is not a known mnemonic/directive, emit it as a label then parse rest of line as actual instruction
- Both LLVM-style (with colons) and xas99-style (without) produce identical object files
- Verified with `clang --target=tms9900 -c test.S -o test.o`

---

*Project Journal - Last Updated: January 4, 2026*
