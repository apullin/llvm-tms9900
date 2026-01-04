# LLVM Backend for TMS9900 CPU

This project implements an LLVM backend for the Texas Instruments TMS9900
microprocessor, the CPU used in the TI-99/4A home computer.

## Project Structure

```
llvm-99/
├── README.md                           # This file
├── tms9900_reference.txt               # Extracted CPU reference data
├── TMS_9900_Microprocessor_Data_Manual_May76.pdf  # Original TI manual
└── llvm-tms9900/
    └── lib/Target/TMS9900/             # LLVM backend source
        ├── TMS9900.td                  # Top-level TableGen description
        ├── TMS9900RegisterInfo.td      # Register definitions
        ├── TMS9900CallingConv.td       # Calling convention
        ├── TMS9900InstrFormats.td      # Instruction format definitions
        ├── TMS9900InstrInfo.td         # Instruction definitions
        ├── TMS9900*.h/cpp              # C++ implementation files
        ├── TargetInfo/                 # Target registration
        └── CMakeLists.txt              # Build configuration
```

## TMS9900 Architecture Summary

The TMS9900 is a 16-bit microprocessor with unique characteristics:

- **Workspace Pointer Architecture**: Instead of hardware registers, the TMS9900
  uses 16 words in RAM (pointed to by the Workspace Pointer) as its "registers"
- **16-bit data and instruction words**
- **Big-endian byte ordering**
- **65KB address space**
- **No hardware stack** - must be implemented in software

### Register Model (as exposed to LLVM)

| Register | Usage |
|----------|-------|
| R0-R9    | General purpose (caller-saved) |
| R10      | Stack Pointer (SP) |
| R11      | Link Register (return address, set by BL) |
| R12      | CRU Base Address |
| R13-R15  | Callee-saved / Context switch storage |

### Calling Convention

- **Arguments**: R0-R3 (first 4 words), then stack
- **Return value**: R0 (16-bit), R0:R1 (32-bit)
- **Stack**: Grows downward, 2-byte aligned
  - Push: `DECT R10` then `MOV Rx,*R10`
  - Pop: `MOV *R10+,Rx`

## Building

This backend is designed to be integrated into LLVM 18. Steps:

1. Clone LLVM 18 source
2. Copy the `llvm-tms9900/lib/Target/TMS9900` directory into LLVM's `lib/Target/`
3. Add `TMS9900` to LLVM's target list in:
   - `llvm/CMakeLists.txt` (LLVM_ALL_TARGETS)
   - `llvm/lib/Target/CMakeLists.txt`
4. Build LLVM with TMS9900 enabled:
   ```bash
   cmake -G Ninja -DLLVM_TARGETS_TO_BUILD="TMS9900" ../llvm
   ninja
   ```

## Usage (Future)

Once built, compile C to TMS9900 assembly:

```bash
clang -target tms9900 -S hello.c -o hello.asm
```

Then assemble with xas99 (from xdt99 toolkit):
```bash
xas99.py hello.asm -o hello.bin
```

## Current Status

**Phase 1 - Skeleton** (Current)
- [x] Register definitions
- [x] Basic instruction formats
- [x] Core arithmetic/logical instructions (register-to-register)
- [x] Calling convention definition
- [x] Frame lowering basics
- [ ] Memory addressing modes (indirect, indexed, symbolic)
- [ ] Complete instruction selection patterns
- [ ] Working code generation for simple functions

**Phase 2 - Functional**
- [ ] All addressing modes implemented
- [ ] Load/store operations
- [ ] Complete branch/compare logic
- [ ] Working function calls

**Phase 3 - Usable**
- [ ] Multiply/divide support
- [ ] Byte operations
- [ ] Optimization passes
- [ ] Testing on ti99sim or real hardware

## Testing

Recommended test environments:
- **ti99sim** - TI-99/4A simulator (may support bare TMS9900)
- **MAME** - Can emulate TMS9900 with custom configurations
- **Classic99** - Windows-based TI-99/4A emulator
- **js99er.net** - Browser-based emulator

## Resources

- [TMS9900 Data Manual (1976)](TMS_9900_Microprocessor_Data_Manual_May76.pdf)
- [xdt99 Cross-development tools](https://github.com/endlos99/xdt99)
- [LLVM Backend Tutorial (Cpu0)](https://jonathan2251.github.io/lbd/)
- [LLVM MSP430 Backend](https://github.com/llvm/llvm-project/tree/main/llvm/lib/Target/MSP430)

## License

This project follows LLVM's Apache 2.0 license with LLVM Exceptions.
