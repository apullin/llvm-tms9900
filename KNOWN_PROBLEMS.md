# TMS9900 LLVM Backend - Known Problems

This document tracks known bugs, deficiencies, and quirks in the TMS9900 LLVM backend.

## Critical Bugs

(None currently open.)

### 1. SETGE/SETLE Signed Comparison Bug

**Status**: FIXED
**Severity**: Critical - broke signed `>=` and `<=` comparisons
**Symptom**: ball2.c bouncing ball oscillated in place instead of bouncing off walls

**Description**:
Signed `>=` comparisons previously generated incorrect code. The condition was effectively inverted.

**Fix Summary** (implemented across several commits):
- `LowerBR_CC()` and `LowerBRCOND()` now correctly order operands for the
  TMS9900 `C src,dst` instruction (flags = dst - src).
- `SELECT16` custom inserter uses CMPBR pseudo-instructions that keep the
  compare and branch adjacent, preventing flag clobber.
- `expandPostRAPseudo()` expands CMPBR with SETGE as JEQ+JGT, SETLE as JEQ+JLT.
- For the common inverted-branch pattern, SETGE is lowered as "JLT to false",
  which is a single-instruction equivalent.
- `reverseBranchCondition()` now inverts CMPBR condition codes via
  `ISD::getSetCCInverse()`, enabling the branch folder to reorder blocks.

**Verification**: 24/24 signed comparison tests pass in the TMS9900 emulator
(test_all_cmp.c covering ge, le, gt, lt, eq, ne with positive, negative, and
zero operands).

---

### 2. MOVB Byte Ordering Bug

**Status**: FIXED
**Severity**: Critical - broke all byte I/O

**Description**:
TMS9900 MOVB instruction moves the HIGH byte of a register, but the compiler
previously put byte values in the LOW byte.

**Fix Summary**:
- `LowerSTORE()` inserts SHL by 8 before BYTE_STORE to shift the value into
  the high byte position.
- `LowerLOAD()` inserts SRL by 8 after BYTE_LOAD to shift the loaded byte
  from the high position to the low position.
- `setTruncStoreAction(MVT::i16, MVT::i8, Custom)` and
  `setLoadExtAction(ISD::EXTLOAD/ZEXTLOAD/SEXTLOAD, MVT::i16, MVT::i8, Custom)`
  ensure these custom lowerings are triggered.

**Verification**: `store_byte` generates `SLA R1,8; MOVB R1,*R0` and
`load_byte` generates `MOVB *R0,R0; SRL R0,8` -- correct byte positioning.

---

## Non-Critical Issues

### 3. reverseBranchCondition

**Status**: FIXED
**Severity**: Was Medium

**Description**:
`TMS9900InstrInfo::reverseBranchCondition()` previously always returned `true`
(failure). It now works for both simple conditional branches (using
`getOppositeCondBranchOpcode()`) and CMPBR pseudo-instructions (using
`ISD::getSetCCInverse()`). This enables the branch folder to optimize block
layout.

---

### 4. Assembly Output Requires Filtering for xas99

**Status**: WONTFIX (documented workaround)
**Severity**: Low - cosmetic

**Description**:
LLVM emits some directives that xas99 doesn't understand:
- `.text` section directives
- `.p2align` alignment directives

**Workaround**:
```bash
grep -v '^\s*\.text' output.s | grep -v '^\s*\.p2align' > clean.s
```

---

### 5. No 32-bit Multiplication/Division

**Status**: OPEN
**Severity**: Low - rarely needed on TMS9900

**Description**:
32-bit multiply and divide operations are not implemented. The TMS9900 MPY instruction only provides 16x16->32 multiplication.

---

## Test Status

| Test | Status | Notes |
|------|--------|-------|
| cart.c | WORKS | Basic cartridge with VDP writes |
| ball2.c | WORKS | Signed comparison bug fixed |
| minimum_working_example | WORKS | Basic function calls and returns |
| test_all_cmp (24 tests) | WORKS | All signed/unsigned comparisons pass |
| benchmark suite (9 programs) | WORKS | All halt correctly at O2 |

---

## Architecture Notes

### TMS9900 Comparison Quirks

The TMS9900 `C` (Compare) instruction:
- Syntax: `C source, dest`
- Computes: `dest - source` for status flags
- Sets: EQ (equal), GT (greater than, signed), LT (less than, signed), etc.

Available signed comparison jumps:
- `JEQ` - Jump if Equal (A == B)
- `JNE` - Jump if Not Equal (A != B)
- `JGT` - Jump if Greater Than (A > B, signed)
- `JLT` - Jump if Less Than (A < B, signed)

NOT available for signed:
- `JGE` - Jump if Greater or Equal (must use JGT + JEQ)
- `JLE` - Jump if Less or Equal (must use JLT + JEQ)

Available unsigned comparison jumps:
- `JH` - Jump if High (A > B, unsigned)
- `JL` - Jump if Low (A < B, unsigned)
- `JHE` - Jump if High or Equal (A >= B, unsigned)
- `JLE` - Jump if Low or Equal (A <= B, unsigned)

---

## Example Build Instructions

### Prerequisites

1. **LLVM Build**: The TMS9900 backend must be built first:
   ```bash
   cd llvm-project/build
   cmake -G Ninja -DLLVM_TARGETS_TO_BUILD="TMS9900" \
         -DLLVM_ENABLE_PROJECTS="clang;lld" \
         -DCMAKE_BUILD_TYPE=Release ../llvm
   ninja
   ```

2. **xdt99 Tools**: For assembling and creating disk/cartridge images (see xdt99 section below)

### Building Cart Examples

```bash
cd cart_example

# Build all cartridges (cart.bin, ball.bin)
make

# Or build individually:
# Compile C to object file
../llvm-project/build/bin/clang --target=tms9900 -O2 -fno-builtin -c cart.c -o cart.o

# Link with cartridge linker script
../llvm-project/build/bin/ld.lld -T cart.ld cart.o -o cart.elf

# Extract raw ROM binary
../llvm-project/build/bin/llvm-objcopy -O binary \
    --only-section=.cart_header \
    --only-section=.cart_entry \
    --only-section=.text \
    --only-section=.rodata \
    cart.elf cart.bin
```

### Viewing Generated Assembly

```bash
# Generate assembly from C (for debugging)
../llvm-project/build/bin/clang --target=tms9900 -O2 -S cart.c -o cart.s

# Disassemble an ELF file
../llvm-project/build/bin/llvm-objdump -d cart.elf

# View sections and symbols
../llvm-project/build/bin/llvm-readelf -S -s cart.elf
```

### Reproducing the ball2 Bug

```bash
cd cart_example

# Compile ball2.c
../llvm-project/build/bin/clang --target=tms9900 -O2 -S ball2.c -o /tmp/ball2.s

# Look at the right-wall comparison (should be around line 175-180)
grep -A5 "ball_x,R1" /tmp/ball2.s

# You'll see:
#   C    R0,R1
#   JGT  LBB1_17      <- This should be JLT for correct >= behavior
#   JMP  LBB1_16
```

---

## xdt99 Tools

**xdt99** is the cross-development toolkit for TI-99/4A, providing assembler (xas99), disassembler (xda99), disk manager (xdm99), and other utilities.

### Online Documentation

- **Main Documentation**: https://endlos99.github.io/xdt99/
- **GitHub Repository**: https://github.com/endlos99/xdt99

### Local Installation

```
/Users/andrewpullin/personal/ti99/xdt99/
```

Key tools:
- `xas99.py` - TMS9900 assembler (understands TI-style syntax)
- `xda99.py` - Disassembler
- `xdm99.py` - Disk image manager
- `xga99.py` - GPL assembler

### Example xas99 Usage

```bash
# Assemble a .asm file
python3 /Users/andrewpullin/personal/ti99/xdt99/xas99.py -b myprogram.asm -o myprogram.bin

# Assemble with listing
python3 /Users/andrewpullin/personal/ti99/xdt99/xas99.py -L mylist.lst -b myprogram.asm

# Note: LLVM output needs filtering before xas99 can process it:
grep -v '^\s*\.text' llvm_output.s | grep -v '^\s*\.p2align' > clean.s
python3 /Users/andrewpullin/personal/ti99/xdt99/xas99.py -b clean.s -o program.bin
```

### Running in Emulator

The `.bin` files produced can be loaded in MAME or js99er.net:
- **js99er.net**: https://js99er.net/ (browser-based, supports cartridge ROMs)
- **MAME**: Use the `ti99_4a` driver with cartridge ROM

---

*Last Updated: 2026-02-06*
