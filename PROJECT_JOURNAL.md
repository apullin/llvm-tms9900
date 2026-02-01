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

### 2026-01-04 DONE .org directive support in AsmParser

**What**: Added `.org` directive parsing to set the assembly origin address.

**Where**: `llvm/lib/Target/TMS9900/AsmParser/TMS9900AsmParser.cpp` - `parseDirective()`, `parseDirectiveOrg()`

**Why**: TI-99/4A cartridges must be placed at specific addresses (0x6000 for ROM). The `.org` directive (and xas99's `AORG`) allows setting this without linker scripts.

**Technical notes**:
- Uses `getStreamer().emitValueToAlignment()` for padding
- Supports both `.org` (LLVM standard) and `AORG` (xas99 dialect)
- Expression evaluation handled by LLVM's expression parser

---

### 2026-01-04 DONE .include directive support in AsmParser

**What**: Added `.include "filename"` directive to include external assembly files.

**Where**: `llvm/lib/Target/TMS9900/AsmParser/TMS9900AsmParser.cpp` - `parseDirectiveInclude()`

**Why**: Enables modular assembly code structure - wrapper files can include compiled C output.

**Technical notes**:
- Uses LLVM's SourceMgr infrastructure
- Searches relative to current file's directory
- Proper error handling for missing files

---

### 2026-01-04 DISCOVERY LLD relocation endianness bug for TMS9900

**What**: Discovered LLD applies relocations in little-endian, but TMS9900 is big-endian. Object files are correct, but linked output has byte-swapped addresses.

**Where**: LLD's ELF linker - no TMS9900-specific code exists. Affects all R_TMS9900_16 relocations.

**Why**: LLD's generic relocation handling defaults to little-endian. Without target-specific code, symbol addresses are written with wrong byte order.

**Technical notes**:
- Object file (cart2.o) shows correct big-endian format: `00 10` followed by `cartridge_main` placeholder
- Linked ELF shows byte-swapped result: addresses in little-endian
- Currently using EM_NONE (0) as machine type - need EM_TMS9900
- Fix requires: new `lld/ELF/Arch/TMS9900.cpp` with `relocate()` using big-endian writes
- Similar to how MSP430 handles this (also 16-bit)

---

### 2026-01-04 PLANNING LLD big-endian relocation support TODO

**What**: Created implementation plan for proper TMS9900 LLD support.

**Where**: New files needed in `lld/ELF/Arch/` and modifications to `lld/ELF/Target.cpp`

**Why**: Enable proper linking of TMS9900 object files with correct big-endian address encoding.

**Tasks identified**:
1. Add EM_TMS9900 to `llvm/include/llvm/BinaryFormat/ELF.h`
2. Create `lld/ELF/Arch/TMS9900.cpp` with `relocate()` using `write16be()`
3. Add `getTMS9900TargetInfo()` declaration to `lld/ELF/Target.h`
4. Add EM_TMS9900 case to `lld/ELF/Target.cpp`
5. Update `lld/ELF/Arch/CMakeLists.txt`
6. Update `TMS9900ELFObjectWriter.cpp` to use `ELF::EM_TMS9900`

---

### 2026-01-05 DONE LLD linker support for TMS9900

**What**: Implemented TMS9900 target support in LLD with proper big-endian relocations.

**Where**:
- `lld/ELF/Arch/TMS9900.cpp` (new file)
- `lld/ELF/Target.h`, `lld/ELF/Target.cpp`, `lld/ELF/CMakeLists.txt`
- `llvm/include/llvm/BinaryFormat/ELF.h` (added EM_TMS9900 = 0x99)
- `llvm/lib/Target/TMS9900/MCTargetDesc/TMS9900ELFObjectWriter.cpp`

**Why**: Complete the native LLVM toolchain - can now compile, assemble, and link TMS9900 programs without external tools.

**Example workflow**:
```bash
clang --target=tms9900 -c startup.S -o startup.o
clang --target=tms9900 -c main.c -o main.o
ld.lld -T linker.ld startup.o main.o -o program.elf
llvm-objcopy -O binary program.elf program.bin
```

**Technical notes**:
- Machine type EM_TMS9900 = 0x99 (153) - fits nicely with TI processor family (near EM_TI_C6000 etc.)
- Relocations use `write16be()` for proper big-endian address encoding
- Supports R_TMS9900_16 and R_TMS9900_PCREL_8/16 relocation types

---

### 2026-01-05 FIX Format8 instruction encoding bug

**What**: Fixed instruction encoding for Format8 instructions (LI, AI, ANDI, ORI, CI) and JMP fixup application.

**Where**:
- `TMS9900InstrFormats.td` - Format8/Format8_Cmp class definitions
- `TMS9900InstrInfo.td` - Instruction definitions using Format8
- `TMS9900AsmBackend.cpp` - applyFixup for pcrel_8

**Why**: Instructions were encoding with wrong byte/bit positions:
- LI R12,0 produced 0x200C instead of 0x02C0 (opcode and register swapped)
- JMP loop produced 0xFF00 instead of 0x10FF (displacement in wrong byte)

**Technical notes**:
- Format8 register is in bits 7-4 (not 3-0), subop in bits 3-0
- Changed Format8 parameter from 8-bit opcode to 4-bit subop
- JMP fixup must write displacement to byte 1 (low byte in big-endian), not byte 0

---

### 2026-01-05 DONE TMS9900 disassembler for llvm-objdump

**What**: Implemented disassembler enabling `llvm-objdump -d` support for TMS9900 ELF files.

**Where**:
- `llvm/include/llvm/Object/ELFObjectFile.h` - Added EM_TMS9900 to ELF triple mapping
- `llvm/lib/Target/TMS9900/Disassembler/TMS9900Disassembler.cpp` (new file)
- `llvm/lib/Target/TMS9900/Disassembler/CMakeLists.txt` (new file)
- `llvm/lib/Target/TMS9900/CMakeLists.txt` - Added Disassembler subdirectory

**Why**: Complete the toolchain - allows inspecting compiled code, debugging, and round-trip verification (compile → disassemble → verify).

**Technical notes**:
- Instruction decoding by format: Format8 (LI/AI/ANDI/ORI/CI), Format9 (LWPI/LIMI), Format6 (jumps), Format7 (shifts), Format3 (single operand), Format1 (dual operand)
- Key challenge: TableGen-generated instruction names have suffixes (e.g., `ABSr`, `MOVrr`, `SRAri`) not just base names
- Special-cased BL @symbol - our encoding puts Ts=0 with target in second word, not standard Format3 addressing
- MOVB partially supported (register-to-indirect mode); operand order may be swapped in print output
- BLWP and X instructions not implemented in current TableGen, so not disassembled
- Successfully decodes cart_example: LWPI, LI, BL, JMP, CLR, MOVB all working

---

### 2026-01-06 FIX Critical Format8 encoding bug (LI/AI/ANDI/ORI/CI)

**What**: Found and fixed critical bug where register and sub-opcode fields were swapped in Format8 instructions. `LI R10, 0x83FE` was generating `0x02A0` (which is `STWP R0`) instead of `0x020A`.

**Where**: `TMS9900InstrFormats.td` lines 552-556 (Format8 class) and lines 573-576 (Format8_Cmp class)

**Why**: This broke stack pointer initialization (`LI R10, 0x83FE`) and consequently all function calls. The TMS9900 Format VIII encoding is `0000 0010 ssss rrrr` where ssss=sub-opcode, rrrr=register. We had them swapped.

**Technical notes**:
- Before: `Inst{7-4} = rd; Inst{3-0} = subop;` → LI R10 = 0x02A0 (STWP R0!)
- After: `Inst{7-4} = subop; Inst{3-0} = rd;` → LI R10 = 0x020A (correct)
- Verified with tms9900-trace: function calls now work correctly at -O0
- Audited ALL instruction formats against `tms9900_reference.txt` - no other encoding bugs found
- Affected instructions: LI (subop=0), AI (subop=2), ANDI (subop=4), ORI (subop=6), CI (subop=8)

---

### 2026-01-06 FIX Disassembler crash in printInstruction

**What**: Fixed multiple crashes in the TMS9900 disassembler caused by incorrect operand counts for instructions with tied constraints.

**Where**: `TMS9900Disassembler.cpp` - Format8, Format3, Format7, and Format1 auto-increment handling

**Why**: Instructions with tied operands (e.g., `$rd = $rs` constraints) require the register to be added multiple times to the MCInst. The printer expects operands at specific indices based on the instruction definition.

**Technical notes**:
- **Format8 (AI/ANDI/ORI)**: Have `(outs $rd), (ins $rs, $imm)` with `$rd = $rs` - need 3 operands (reg, reg, imm), was only adding 2
- **Format3 (INC/DEC/NEG/etc)**: Have `(outs $rd), (ins $rs)` with `$rd = $rs` - need 2 operands (reg, reg), was only adding 1
- **Format3 opcode mapping**: Was completely wrong! INC/INCT/DEC/DECT opcodes were mixed up with ABS/SWPB/etc. Fixed to match TMS9900InstrInfo.td definitions
- **Format7 (shifts)**: Have `(outs $rd), (ins $rs, $cnt)` with `$rd = $rs` - need 3 operands (reg, reg, imm), was only adding 2
- **Format1 auto-increment**: MOVpim has `(outs $rd, $rs_wb), (ins $rs)` - need 3 operands, was only adding 2

---

### 2026-01-06 FIX Stack spills now work (MOV_FI pseudo expansion)

**What**: Fixed critical bug where register spills to the stack would crash with "Not supported instr: MCInst 275". Any C code with nested function calls or register pressure would fail.

**Where**: `TMS9900MCInstLower.cpp` - added expansion of MOV_FI_Store and MOV_FI_Load pseudo instructions

**Why**: The `MOV_FI_Store` and `MOV_FI_Load` pseudo instructions (used by `storeRegToStackSlot`/`loadRegFromStackSlot`) were being passed directly to MCCodeEmitter, which can't encode pseudos. They need to be expanded to real `MOVmx`/`MOVxm` instructions first.

**Technical notes**:
- `MOV_FI_Store`: (ins base, offset, rs) → `MOVmx`: (ins offset, ri, rs) - operand reordering required
- `MOV_FI_Load`: (outs rd), (ins base, offset) → `MOVxm`: (outs rd), (ins offset, ri) - operand reordering required
- MCInst 275 = MOV_FI_Store opcode number
- Now cart_example with `vdp_write_at()` helper function compiles correctly
- This was blocking any real C development - functions that call other functions need to save registers

---

### 2026-01-06 FIX Critical instruction encoding bug - second word not emitted

**What**: Fixed critical bug where `Format1_SymLoad`, `Format1_SymStore`, `Format1_IdxLoad`, and `Format1_IdxStore` instruction classes weren't emitting the second instruction word (address/offset). All global variable accesses produced addresses of 0x0000.

**Where**: `TMS9900InstrFormats.td` lines 300-436 - all four Format1_*Load and Format1_*Store classes with memory addressing

**Why**: These instruction classes had `bits<16> Inst` but the instructions are 4 bytes (2 words). The address/offset operand was defined but never assigned to instruction bits. The second word was just zeros - no relocations were generated.

**Technical notes**:
- Changed `bits<16> Inst` to `bits<32> Inst` in all four classes
- Added `let Inst{31-16} = addr;` (or `= offset;`) to emit the second word
- `BL_sym` already had this correct: `bits<32> Inst` with `Inst{31-16} = target`
- ball.c bouncing ball demo now has 36 relocations instead of 1
- Global variables at addresses 0x2000+ now appear correctly in linked binary
- MOVma/MOVam/MOVmx/MOVxm all affected (word + byte variants)

---

### 2026-01-06 FIX Disassembler crashes on symbolic/indexed addressing modes

**What**: Disassembler was crashing when encountering Format 1 instructions with symbolic or indexed addressing modes (Ts=2 or Td=2). Added proper handling for these cases.

**Where**: `TMS9900Disassembler.cpp` lines 523-610 - added four new decoding paths

**Why**: Format 1 instructions with memory addressing (MOV @addr,Rd, MOV Rs,@addr, MOV @offset(Rs),Rd, MOV Rs,@offset(Rd)) need to read the second word for address/offset and decode accordingly.

**Technical notes**:
- Symbolic source (Ts=2, S=0, Td=0): MOV @addr,Rd → MOVam
- Symbolic dest (Td=2, D=0, Ts=0): MOV Rs,@addr → MOVma
- Indexed source (Ts=2, S!=0, Td=0): MOV @offset(Rs),Rd → MOVxm
- Indexed dest (Td=2, D!=0, Ts=0): MOV Rs,@offset(Rd) → MOVmx
- Must distinguish symbolic (S=0/D=0) from indexed (S!=0/D!=0) addressing

---

### 2026-01-06 FIX Disassembler crashes on arithmetic instructions (tied operands)

**What**: Disassembler crashed on A/S/SOC/SZC register-to-register instructions due to tied operand handling. These instructions have `$rd = $rs1` constraint requiring the destination register to appear twice in the MCInst.

**Where**: `TMS9900Disassembler.cpp` lines 427-465 - register-to-register Format 1 decoding

**Why**: Instructions like `A R2,R1` have pattern `(outs $rd), (ins $rs1, $rs2)` with `$rd = $rs1` tied constraint. MCInst needs 3 register operands but only 2 were being added. Compare instructions (C/CB) have no output and don't need tied handling.

**Technical notes**:
- Added `needsTiedOperand` flag for A/S/SOC/SZC (opcodes 0x4,0x6,0xA,0xE and byte variants)
- Added `isCompare` flag for C/CB (opcodes 0x8,0x9) which have no output register
- MOV doesn't need tied operand - it's a simple copy not read-modify-write
- ball.elf and cart.elf now disassemble completely without crashes

---

### 2026-01-06 DONE TMS9900 branch relaxation

**What**: Implemented automatic branch relaxation for JMP instructions that exceed the 8-bit signed displacement range (~256 bytes). Out-of-range JMPs are converted to B @addr (4-byte absolute branch).

**Where**:
- `TMS9900InstrInfo.td` - added `B_sym` instruction and `brtarget16` operand type
- `TMS9900MCCodeEmitter.cpp` - added `getBranchTarget16Encoding()` method
- `TMS9900AsmBackend.cpp` - implemented `fixupNeedsRelaxation()`, `fixupNeedsRelaxationAdvanced()`, `relaxInstruction()`, and fixed `mayNeedRelaxation()` to only return true for JMP

**Why**: ball2.c (bouncing ball demo with hit counters) was failing with "fixup value out of range" because the main() function exceeded the ~256 byte conditional branch limit. TMS9900 Format 6 jumps have 8-bit signed displacement.

**Technical notes**:
- `B @addr` encoding: opcode 0x0440 with Ts=10 (symbolic), S=0, followed by 16-bit address
- `B_sym` is a 4-byte instruction (vs 2-byte JMP)
- Only JMP can be relaxed; conditional branches (JEQ/JNE/JGT/JL/etc.) cannot be relaxed in LLVM's MC layer because it would require emitting multiple instructions (inverted branch + B @addr)
- Code generator emits patterns like `JL skip; JMP target` so relaxing JMP handles most cases
- Currently ALL JMPs are relaxed (fixupNeedsRelaxationAdvanced returns true for unresolved fixups) - could be optimized to only relax when actually out of range

---

### 2026-01-09 DONE Add libtms9900 with picolibc - first libm implementation

**What**: Created libtms9900/ runtime library providing compiler builtins and math functions. Includes custom compact sinf/cosf implementation (1.2KB vs 7KB+ standard picolibc).

**Where**:
- `libtms9900/builtins/` - 32-bit integer ops (mul32.asm, div32.asm, shift32.asm) and soft-float from compiler-rt
- `libtms9900/libm/` - Math library with sincosf_tiny.c and picolibc sources
- `libtms9900/picolibc/` - Upstream picolibc source (TODO: convert to submodule)

**Why**: Needed soft-float and libm support for floating-point C code. Picolibc chosen over newlib for smaller size. Custom sin/cos written because standard picolibc's Payne-Hanek range reduction pulls in 8KB+ of code.

**Technical notes**:
- Float32 only - no 64-bit double support to minimize code size
- Compact sinf/cosf uses Cody-Waite range reduction with extended-precision π/2 constants
- Tradeoff: Full precision for |x| < 10^4, reduced precision (~4 digits) for |x| > 10^6
- libm.a sizes: -O1=21,522B, -Os=20,980B, -O2=21,546B, -O3=25,816B (sqrtf explodes to 4.4KB at -O3)
- powf is 5.1KB (24% of library) with 126 soft-float calls - TODO: optimize for small integer exponents
- Documented full symbol table with sizes in libtms9900/README.md

---

### 2026-01-07 DONE Add BLWP/XOP/CRU instruction support

**What**: Added TMS9900 system and I/O instructions: BLWP (branch and load workspace pointer), XOP (extended operation), and CRU bit I/O (LDCR, STCR, SBO, SBZ, TB).

**Where**:
- `TMS9900InstrFormats.td` - new Format9/Format12 instruction classes
- `TMS9900InstrInfo.td` - instruction definitions
- `TMS9900Disassembler.cpp` - decoding logic
- `TMS9900AsmParser.cpp` - parsing support

**Why**: These instructions are essential for TI-99/4A system programming - BLWP for context switches, XOP for system calls, CRU for peripheral I/O (keyboard, cassette, etc).

---

### 2026-01-07 DONE Add CKOF/CKON/LREX instruction support

**What**: Added clock control and interrupt instructions: CKOF (clock off), CKON (clock on), LREX (load or restart execution).

**Where**: `TMS9900InstrInfo.td`, `TMS9900Disassembler.cpp`

**Why**: Complete the TMS9900 instruction set for system-level programming.

---

### 2026-01-08 FIX Compare/branch adjacency - CMPBR pseudo

**What**: Introduced CMPBR pseudo-instruction to keep compare and branch adjacent. Select CMPBR when BR_CC is glued to CMP, expand post-RA to C/CI + Jcc.

**Where**:
- `TMS9900ISelDAGToDAG.cpp` - pattern matching for CMPBR selection
- `TMS9900InstrInfo.cpp` - post-RA expansion
- `TMS9900InstrInfo.td` - CMPBR pseudo definition

**Why**: TMS9900 conditional branches test the status register set by the previous compare. If other flag-setting instructions interleave between compare and branch, the condition is corrupted.

**Technical notes**:
- CMPBR bundles compare opcode + condition + operands into single pseudo
- Expanded after register allocation when no more instructions can be inserted
- Handles both register and immediate comparisons

---

### 2026-01-08 FIX Model flags clobber for MOV memory ops

**What**: Marked memory MOV/MOVB instructions as defining the status register (ST). Changed BRCOND lowering to go via CMP+BR_CC to preserve flag dependencies.

**Where**: `TMS9900InstrInfo.td` (Defs = [ST] on MOV patterns), `TMS9900ISelLowering.cpp`

**Why**: TMS9900's MOV instruction sets status flags (compare result with 0). If this isn't modeled, the scheduler might move a MOV between a compare and its branch, corrupting the condition.

---

### 2026-01-08 DONE Add NOP alias

**What**: Added NOP as alias for JMP $+2 (jump to next instruction). Prevent relaxation of JMP with zero offset so inline asm NOP stays 2 bytes.

**Where**: `TMS9900InstrInfo.td`, `TMS9900AsmBackend.cpp`

**Why**: Convenience for assembly programmers and inline asm. TMS9900 has no hardware NOP - JMP $+2 is the standard idiom.

---

### 2026-01-08 FIX Tied frame index for i8 truncation

**What**: Fixed register allocation crash when truncating i16 to i8 with frame index operands. Tied operand constraints weren't properly handled.

**Where**: `TMS9900InstrInfo.td` - i8 store patterns with frame index

**Why**: Code like `char x = (char)value; stack_var = x;` was crashing during register allocation.

---

### 2026-01-09 FIX Frame index scratch register clobbers

**What**: Fixed bug where materializing frame index addresses would clobber live registers. Added proper scratch register handling in frame lowering.

**Where**: `TMS9900RegisterInfo.cpp`, `TMS9900FrameLowering.cpp`

**Why**: Functions with multiple stack variables were getting corrupted values because the frame index materialization code was reusing registers without checking liveness.

---

### 2026-01-09 FIX SETCC/branch lowering overhaul

**What**: Major rework of comparison and branch lowering. Use -1 (all ones) for boolean true, custom SETCC for i16/i32, restrict indexed addressing base registers to avoid R0.

**Where**:
- `TMS9900ISelLowering.cpp` - custom SETCC lowering, boolean representation
- `TMS9900InstrInfo.td` - IdxRegs register class excluding R0

**Why**: Multiple issues: (1) compare+branch weren't staying adjacent, (2) i32 BR_CC wasn't lowering correctly, (3) R0 used as index register encodes as symbolic addressing (0 means "no register").

**Technical notes**:
- -1 booleans allow AND/OR to work correctly with condition results
- i32 BR_CC now lowers via SETCC+BRCOND chain
- Frame index pseudos also restricted to IdxRegs

---

### 2026-01-09 DONE LEAfi pseudo for frame address

**What**: Added LEAfi pseudo-instruction to compute frame index addresses (base + offset) into a register.

**Where**: `TMS9900InstrInfo.td`, `TMS9900ISelLowering.cpp`

**Why**: Needed for passing addresses of stack variables to functions (e.g., `scanf(&x)`).

---

### 2026-01-10 FIX CMPBR in branch analysis

**What**: Taught analyzeBranch/insertBranch/removeBranch to handle CMPBR pseudo instructions alongside regular branches.

**Where**: `TMS9900InstrInfo.cpp` - branch analysis methods

**Why**: The branch optimization passes need to understand CMPBR to correctly analyze control flow and avoid breaking the compare+branch bundles.

---

### 2026-01-10 FIX Avoid inverting non-invertible fallthrough

**What**: Fixed incorrect control flow when a branch's fallthrough target couldn't be inverted. Was generating incorrect code for certain if/else patterns.

**Where**: `TMS9900InstrInfo.cpp` - branch inversion logic

**Why**: Some branch patterns have asymmetric invertibility - e.g., JL (jump if less) inverts to JGE, but complex compound conditions might not have a simple inverse.

---

### 2026-01-11 FIX SELECT16 compare+branch gluing

**What**: Fixed select (ternary operator) lowering to properly glue compare and conditional move operations.

**Where**: `TMS9900ISelLowering.cpp` - SELECT_CC lowering

**Why**: Code like `x = (a < b) ? c : d` was generating incorrect results because the compare flags were being clobbered before the conditional move.

---

### 2026-01-12 FIX Skip call-frame pseudos when reserved

**What**: Fixed crash when call-frame setup/destroy pseudos appeared with reserved call frame (no SP adjustment needed). Skip these pseudos during expansion.

**Where**: `TMS9900FrameLowering.cpp`

**Why**: Certain calling patterns with small/no stack frames were triggering assertion failures during pseudo expansion.

---

### 2026-01-13 REFACTOR Switch disassembler to TableGen tables

**What**: Major refactor of disassembler from hand-written decoding to TableGen-generated tables. Deleted ~974 lines of manual decoder code.

**Where**:
- `TMS9900Disassembler.cpp` - now uses generated tables with custom decoders for branches/CRU/shifts
- `TMS9900InstrInfo.td` - added DecoderMethod annotations
- `CMakeLists.txt` - enabled gen-disassembler

**Why**: Hand-written decoder was error-prone and hard to maintain. TableGen tables are auto-generated from instruction definitions, ensuring consistency.

**Technical notes**:
- Custom decoders still needed for: branch targets (PC-relative), CRU bit offsets, R0-count shifts
- R0-count shifts hidden from asm/disasm (use SLA R1,0 not SLA R1,R0)

---

### 2026-01-13 DONE Disassembler coverage expansion

**What**: Added disassembly support for format1/format2 memory ops, format3 (memory operands), byte ops (MOVB, AB, etc.), COC/CZC, and 48-bit format1 mem-to-mem instructions.

**Where**: `TMS9900Disassembler.cpp`, `TMS9900InstrInfo.td`

**Why**: Complete disassembler coverage for all TMS9900 instruction formats.

---

### 2026-01-14 DONE Long-range signed branch expansion

**What**: New TMS9900LongBranch pass expands conditional branches that exceed the signed 8-bit displacement range. Converts `JLT target` (out of range) to `JGE skip; B @target; skip:`.

**Where**:
- `TMS9900LongBranch.cpp` - new MachineFunctionPass
- `TMS9900TargetMachine.cpp` - pass registration

**Why**: TMS9900 conditional branches (JEQ/JNE/JGT/JLT/etc.) have only 8-bit signed displacement (~256 bytes). Large functions need branch expansion.

**Technical notes**:
- Different from MC-layer relaxation which only handles JMP
- Conditional branches require two-instruction sequence (inverted condition + absolute branch)
- Pass runs late, after branch folding

---

### 2026-01-14 DONE Peephole optimizer

**What**: New TMS9900Peephole pass for target-specific optimizations:
- Fold `MOV @x,Ry; INC Ry` into `MOV *Rx+,Ry` (auto-increment)
- Optimize `AI Rx,0` (remove), `AI Rx,1` → `INC`, `AI Rx,2` → `INCT`
- Optimize `LI Rx,0` → `CLR Rx`, `LI Rx,-1` → `SETO Rx`
- Fold `LI Rx,val; XOR Rx,Ry` into `XOR @lit,Ry`

**Where**:
- `TMS9900Peephole.cpp` - new MachineFunctionPass
- Added `-tms9900-disable-peephole` flag for debugging

**Why**: These patterns are common in compiled code but LLVM's generic optimizers don't know about TMS9900-specific instructions like INC/INCT/CLR/SETO.

---

### 2026-01-14 DONE CRU symbol fixups

**What**: Added 8-bit fixup support for CRU single-bit instructions (SBO, SBZ, TB). Allows symbolic bit offsets that the linker resolves.

**Where**: `TMS9900AsmBackend.cpp`, `TMS9900MCCodeEmitter.cpp`, `TMS9900AsmParser.cpp`

**Why**: CRU programming often uses named bit offsets (e.g., `CRU_LED EQU 5; SBO CRU_LED`). Previously only immediate values worked.

---

### 2026-01-14 DONE Inline constant i32 shifts

**What**: Generate inline code for 32-bit shifts by constant amounts instead of always calling runtime library.

**Where**: `TMS9900ISelLowering.cpp`

**Why**: `x << 1` was calling `__ashlsi3` even though inline code is smaller and faster for small constants.

**Technical notes**:
- Shift by 16 uses word swap
- Small shifts (1-3) expand to repeated operations
- Large/variable shifts still use libcall

---

### 2026-01-14 FIX Avoid relaxing unresolved JMPs

**What**: Fixed MC-layer branch relaxation to not relax JMPs with unresolved symbols (external references).

**Where**: `TMS9900AsmBackend.cpp`

**Why**: Was incorrectly relaxing all JMPs including those to external symbols, causing link errors.

---

### 2026-01-15 DONE Scheduling model tags

**What**: Added basic scheduling model annotations (instruction latencies) to TMS9900 instruction definitions.

**Where**: `TMS9900InstrInfo.td`, `TMS9900Schedule.td`

**Why**: Enables LLVM's scheduler to make better decisions about instruction ordering. Foundation for future cycle-count optimization.

---

### 2026-01-17 FIX SELECT16 SSA and CMPBR scheduling

**What**: Fixed issues with SELECT16 pseudo-instruction: wasn't preserving SSA form correctly, and CMPBR pseudos weren't being scheduled properly with their compare inputs.

**Where**: `TMS9900ISelLowering.cpp`, `TMS9900InstrInfo.cpp`

**Why**: Ternary operator code (`a ? b : c`) was generating incorrect results in some cases.

---

### 2026-01-17 CLEANUP Warnings and test cleanup

**What**: Fixed compiler warnings, cleaned up test cases for conditional branch relaxation.

**Where**: Various files

**Why**: Code hygiene.

---

### 2026-02-01 DONE DWARF debug info support

**What**: Enabled DWARF5 debug information emission for the TMS9900 backend. The compiler now produces valid `.debug_info`, `.debug_line`, `.debug_frame`, `.debug_addr`, and `.debug_str` sections when compiling with `-g`.

**Where**:
- `MCTargetDesc/TMS9900MCTargetDesc.cpp`: Set `SupportsDebugInformation = true`, `UsesCFIWithoutEH = true`, `DwarfRegNumForCFI = true`. Added initial CFA frame state (`DW_CFA_def_cfa R10, 0`) in `createTMS9900MCAsmInfo()`. Changed `shouldOmitSectionDirective()` to defer to base class so debug sections get proper directives.
- `TMS9900FrameLowering.cpp`: Added CFI directives in `emitPrologue()` and `emitEpilogue()` — `cfiDefCfaOffset` after stack adjustments, `createOffset` for R11 (return address) save.
- `llvm/include/llvm/BinaryFormat/ELFRelocs/TMS9900.def`: New file defining R_TMS9900_NONE/16/PCREL_8/PCREL_16/8 relocation types (previously only in a local enum in the ELF object writer).
- `llvm/include/llvm/BinaryFormat/ELF.h`: Added `#include "ELFRelocs/TMS9900.def"` enum block.
- `llvm/lib/Object/ELF.cpp`: Added EM_TMS9900 case for relocation name printing.
- `llvm/lib/Object/RelocationResolver.cpp`: Added `supportsTMS9900()`/`resolveTMS9900()` and switch case, so `llvm-dwarfdump` can resolve relocations in `.o` files.
- `TMS9900ELFObjectWriter.cpp`: Removed local relocation enum, now uses `ELF::R_TMS9900_*` from shared header.

**Why**: Enables source-level debugging with GDB. The DWARF info maps addresses to source lines, names functions/variables, and provides frame unwinding data for backtraces.

**Technical notes**:
- DWARF register numbering was already correct in `TMS9900RegisterInfo.td` (R0-R15 = 0-15, PC=16, WP=17, ST=18).
- The key missing piece was the initial CFA rule in the CIE — without `DW_CFA_def_cfa R10, 0`, all `cfiDefCfaOffset` instructions in FDEs failed with "CFA rule was not RegPlusOffset".
- `int` is correctly reported as `DW_ATE_signed` with `byte_size = 0x02` (16-bit).
- The relocation resolver was critical for `llvm-dwarfdump` to work on `.o` files — without it, all string references showed as `()`.
- This serves as a prototype for adding DWARF to other vintage CPU backends (i8085, i8086, etc.).

---

*Project Journal - Last Updated: February 1, 2026*
