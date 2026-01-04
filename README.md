# LLVM Backend for TMS9900 CPU

This project implements an LLVM backend for the Texas Instruments TMS9900
microprocessor, the CPU used in the TI-99/4A home computer.

## Repository Structure

This project spans two repositories:

**LLVM Fork** ([github.com/apullin/llvm-project](https://github.com/apullin/llvm-project), branch `tms9900`):
- The TMS9900 backend inside LLVM 18
- Clang driver support for `--target=tms9900`

**Tools Repository** (this repo - [github.com/apullin/llvm-tms9900-tools](https://github.com/apullin/llvm-tms9900-tools)):
```
llvm-tms9900-tools/
├── README.md                    # This file
├── PROJECT_JOURNAL.md           # Detailed implementation notes
├── llvm2xas99.py                # LLVM asm → xas99 format converter
├── runtime/
│   └── tms9900_rt.asm           # 32-bit math runtime library
├── startup/
│   └── startup.asm              # Bare-metal startup template
├── tests/                       # LLVM IR test cases
└── tms9900_reference.txt        # Quick instruction reference
```

## TMS9900 Architecture Summary

The TMS9900 is a 16-bit microprocessor with unique characteristics:

- **Workspace Pointer Architecture**: Instead of hardware registers, the TMS9900
  uses 16 words in RAM (pointed to by the Workspace Pointer) as its "registers"
- **16-bit data and instruction words**
- **Big-endian byte ordering**
- **64KB address space**
- **No hardware stack** - implemented in software using R10

### Register Model

| Register | Usage |
|----------|-------|
| R0       | Return value |
| R1-R9    | Arguments (first 9 words), caller-saved |
| R10      | Stack Pointer (SP) |
| R11      | Link Register (return address, set by BL) |
| R12      | CRU Base Address / scratch |
| R13-R15  | Callee-saved |

### Calling Convention

- **Arguments**: R1-R9 (first 9 words), then stack
- **Return value**: R0 (16-bit), R0:R1 (32-bit, high:low)
- **32-bit arguments**: Use register pairs (R1:R2, R3:R4, etc.)
- **Stack**: Grows downward, 2-byte aligned
  - Push: `DECT R10` then `MOV Rx,*R10`
  - Pop: `MOV *R10+,Rx`

## Building

### 1. Clone and Build LLVM with TMS9900 Backend

```bash
git clone git@github.com:apullin/llvm-project.git
cd llvm-project
git checkout tms9900

mkdir build && cd build
cmake -G Ninja \
  -DLLVM_TARGETS_TO_BUILD="TMS9900" \
  -DLLVM_ENABLE_PROJECTS="clang" \
  -DCMAKE_BUILD_TYPE=Release \
  ../llvm
ninja clang llc
```

### 2. Clone Tools Repository

```bash
git clone git@github.com:apullin/llvm-tms9900-tools.git
```

## Toolchain Overview

The current toolchain relies on external tools for assembly and linking:

```
┌─────────┐    ┌─────────┐    ┌─────────────┐    ┌─────────┐    ┌──────────┐
│  C code │───▶│  clang  │───▶│ LLVM generic│───▶│llvm2xas99│───▶│  xas99   │───▶ Binary
│ (.c)    │    │         │    │  asm (.s)   │    │  (.py)  │    │  (.py)   │
└─────────┘    └─────────┘    └─────────────┘    └─────────┘    └──────────┘
```

**Why the extra steps?**

LLVM's assembly output uses its own syntax conventions (`.text`, `.word`, `0x1234` hex literals, etc.), but no standalone TMS9900 assembler understands this format. We use [xas99](https://github.com/endlos99/xdt99) from the xdt99 toolkit as our assembler and linker, which expects traditional TI assembler syntax (`DATA`, `>1234` hex literals, `DEF`/`REF` for symbols, etc.).

The `llvm2xas99.py` script bridges this gap by converting LLVM's assembly output to xas99-compatible format.

### xas99 Dialect (Native Support)

The LLVM backend now includes native xas99 dialect support, which outputs assembly that xas99 can directly assemble (with minimal filtering):

```bash
# Compile with xas99 dialect
clang --target=tms9900 -O2 -S -fno-addrsig \
      -mllvm -tms9900-asm-dialect=xas99 test.c -o test.s

# Filter remaining LLVM directives and assemble
grep -v '^\s*\.' test.s > test_clean.s
xas99.py -R test_clean.s -b -o test.bin
```

The xas99 dialect:
- Outputs hex immediates as `>XXXX` format (e.g., `LI R0,>1234`)
- Outputs negative values as two's complement (e.g., `>FFFF` for -1)
- Uses `DEF` for symbol exports
- Uses `BSS` for zero-filled data (not `.zero`)
- Requires `-fno-addrsig` to suppress LLVM's address significance table

**Quirk:** LLVM's MC layer always emits `.text`, `.data`, `.bss`, and `.p2align` directives when switching sections or aligning data. These are deeply embedded in LLVM's machine code infrastructure and cannot be suppressed without writing a custom MCStreamer. The simple workaround is to filter them out:

```bash
grep -v '^\s*\.' test.s > test_clean.s
```

This is a known limitation that will be resolved when we implement a native TMS9900 assembler or direct object emission.

For more complex projects or if you need additional transformations, the `llvm2xas99.py` script provides more comprehensive conversion.

## Usage

### Compile C to TMS9900 Assembly

```bash
# Using clang driver (outputs LLVM-style assembly)
./bin/clang --target=tms9900 -O2 -S hello.c -o hello.s

# Convert to xas99 format
python3 llvm2xas99.py hello.s > hello.asm

# Assemble and link with xas99 (from xdt99 toolkit)
xas99.py -R hello.asm -b -o hello.bin
```

### Compile LLVM IR Directly

```bash
./bin/llc -march=tms9900 -O2 test.ll -o test.s
python3 llvm2xas99.py test.s > test.asm
```

### Using the Runtime Library

For 32-bit operations (multiply, divide, modulo), link with the runtime:

```bash
# Assemble the runtime
xas99.py -R runtime/tms9900_rt.asm -o tms9900_rt.o

# Include in your project
cat your_code.asm runtime/tms9900_rt.asm > combined.asm
xas99.py -R combined.asm -b -o program.bin
```

## Current Status

The backend is **functional** and can compile real C programs.

### Completed Features

- All 16-bit integer operations (add, sub, mul, div, mod, shifts, logic)
- 8-bit operations (promoted to 16-bit internally)
- 32-bit operations via runtime library calls
- All addressing modes (register, indirect, indexed, symbolic, auto-increment)
- Function calls with proper calling convention
- Local variables and stack frame management
- Signed and unsigned comparisons with correct branch generation
- Switch statements via jump tables
- Inline assembly with register constraints
- Interrupt handlers (`__attribute__((interrupt))`)
- Naked functions (`__attribute__((naked))`)

### Runtime Library Functions

| Function | Purpose |
|----------|---------|
| `__mulsi3` | 32-bit multiply |
| `__divsi3` | 32-bit signed divide |
| `__udivsi3` | 32-bit unsigned divide |
| `__modsi3` | 32-bit signed modulo |
| `__umodsi3` | 32-bit unsigned modulo |
| `__ashlsi3` | 32-bit left shift |
| `__lshrsi3` | 32-bit logical right shift |
| `__ashrsi3` | 32-bit arithmetic right shift |

### Not Yet Implemented

- Floating point (would need software float library)
- Direct binary output (requires assembler post-processing)
- Debug info / DWARF

## Testing

Development and testing has been done using [tms9900-trace](https://github.com/apullin/tms9900-trace), a standalone TMS9900 CPU simulator derived from ti99sim. This provides bare CPU execution with flat RAM, which is ideal for testing compiler output in isolation.

### Minimum Working Example

The `tests/` directory contains a minimum working example that exercises the compiler:

```bash
# Run the MWE test (requires xas99 and tms9900-trace)
./tests/run_mwe_test.sh
```

The test (`tests/mwe_test.c`) performs:
1. Dot product of two 5-element arrays → result: 55
2. Bubble sort on one array
3. Dot product again (with sorted array) → result: 86
4. Returns sum (141 = 0x8D)

Expected memory results after execution:
- `result_dot1` = 0x0037 (55)
- `result_dot2` = 0x0056 (86)
- `sorted_array` = {1, 2, 5, 8, 9}
- `R0` = 0x008D (141)

### Manual Testing

```bash
# Assemble to binary
xas99.py -R -b test.asm -o test.bin

# Run through simulator
tms9900-trace -l 0x8000 -e 0x8000 -w 0x8300 -n 1000 test.bin

# With memory dump
tms9900-trace -l 0x8000 -e 0x8000 -w 0x8300 -n 1000 -d 0x8000:256 test.bin
```

**Note:** Full TI-99/4A system testing has not yet been performed. The goal is to support real TI-99/4A programs, but coexistence with the TI-99/4A memory map (GROM, VDP, VRAM, cartridge ROM) is an open question that will be addressed in future project phases.

## Resources

- [xdt99 Cross-development tools](https://github.com/endlos99/xdt99)
- [PROJECT_JOURNAL.md](PROJECT_JOURNAL.md) - Detailed implementation notes
- [LLVM MSP430 Backend](https://github.com/llvm/llvm-project/tree/main/llvm/lib/Target/MSP430) - Similar 16-bit target

## License

This project follows LLVM's Apache 2.0 license with LLVM Exceptions.
