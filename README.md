# LLVM Backend for TMS9900 CPU

A production-quality LLVM backend for the Texas Instruments TMS9900 — the 16-bit CPU in the TI-99/4A home computer. Compiles C, C++ (with STL), and Rust 🦀 to native TMS9900 machine code.

## Highlights

- **Full LLVM 18 toolchain** — clang, opt, lld, llvm-objcopy, assembler, and disassembler. No external tools required.
- **Heavily stress-tested** — 100 Csmith random programs, CoreMark, MiniLZO, sprintf test suite, and 20 hand-written benchmarks (60/60 pass across O0/O1/O2).
- **C++ with STL** — freestanding libc++ headers: `vector`, `string`, `tuple`, `optional`, `unique_ptr`, `bitset`, algorithms, and more. Lambdas, multiple inheritance, move semantics, variadic templates all working.
- **Rust `#![no_std]`** 🦀 — builds with `cargo +tms9900 build -Z build-std=core`. See [Rust Support](#rust-support) below.
- **Hand-tuned runtime library** — assembly-optimized 32-bit multiply/divide/shift, 64-bit arithmetic, `memcpy`/`memset`/`memmove` exploiting auto-increment addressing, and IEEE 754 soft-float.
- **picolibc math** — single-precision `libm` (sin, cos, sqrt, exp, log, pow, ...) built from [picolibc](https://github.com/picolibc/picolibc) sources, with a custom compact sinf/cosf (1.2KB vs 7KB+ standard).

## Quick Start

### Build the toolchain

```bash
git clone git@github.com:apullin/llvm-tms9900.git
cd llvm-tms9900
git submodule update --init --recursive

cd llvm-project
mkdir build && cd build
cmake -G Ninja \
  -DLLVM_TARGETS_TO_BUILD="TMS9900" \
  -DLLVM_ENABLE_PROJECTS="clang;lld" \
  -DCMAKE_BUILD_TYPE=Release \
  ../llvm
ninja
```

### Compile and link

```bash
# Compile C to object file
clang --target=tms9900 -O2 -c main.c -o main.o

# Assemble startup code
clang --target=tms9900 -c startup.S -o startup.o

# Link with LLD
ld.lld -T linker.ld startup.o main.o -o program.elf

# Extract raw binary
llvm-objcopy -O binary program.elf program.bin
```

### Run on emulator

```bash
tms9900-trace -l 0x0000 program.bin -n 100000 -S
```

Testing is done with [tms9900-trace](https://github.com/apullin/tms9900-trace), a standalone TMS9900 CPU simulator.

## Repository Structure

```
llvm-tms9900/
├── llvm-project/                # LLVM 18 fork (submodule, branch: tms9900)
├── libtms9900/
│   ├── builtins/                # Runtime intrinsics (32/64-bit ops, memcpy, soft-float)
│   ├── libm/                    # Math library (picolibc-based, float32)
│   └── picolibc/                # Embedded C library source
├── tests/
│   ├── benchmarks/              # 20 benchmarks (14 C + 6 C++), 60/60 pass
│   └── stress/                  # Csmith, CoreMark, MiniLZO, sprintf
├── cart_example/                # TI-99/4A cartridge demos (Game of Life, etc.)
├── llvm2xas99.py                # LLVM asm → xas99 format converter (legacy)
└── PROJECT_JOURNAL.md           # Detailed development log
```

## Toolchain

The backend produces ELF object files directly — no external assembler needed:

```
 .c / .S  ──▶  clang -c  ──▶  .o (ELF)  ──▶  ld.lld  ──▶  llvm-objcopy  ──▶  .bin
```

All standard LLVM tools work: `opt` for IR optimization, `llc` for code generation, `llvm-objdump` for disassembly, `llvm-size` for section sizes.

## Language Support

### C

Full C11 support in freestanding mode (`-ffreestanding`). All integer sizes through 64-bit, IEEE 754 soft-float, `switch` via jump tables, inline assembly, interrupt handlers, naked functions.

### C++ with STL

Freestanding C++ with header-only libc++ from the LLVM tree:

```bash
clang --target=tms9900 -O2 -ffreestanding -fno-exceptions -fno-rtti \
  -fno-threadsafe-statics -nostdinc++ \
  -isystem libcxx_config -isystem llvm-project/libcxx/include \
  -c program.cpp -o program.o
```

**Working STL containers and utilities**: `vector`, `string`, `pair`, `tuple`, `optional`, `string_view`, `unique_ptr`, `initializer_list`, `bitset`, `numeric_limits`, `array`, and algorithms (`find`, `count`, `reverse`, `min`, `max`, `sort`).

**Working language features**: lambdas (all capture modes), multiple inheritance with virtual dispatch, move semantics, perfect forwarding, variadic templates, structured bindings, `constexpr`, `enum class`, static local init.

A minimal C++ runtime (`cxxrt.cpp`) provides `operator new`/`delete` and `__cxa_*` stubs. See `tests/benchmarks/` for examples.

### Rust Support

Rust `#![no_std]` cross-compilation is supported via a custom Rust 1.81.0 build. 🦀

**Setup** (one-time):

1. Clone Rust 1.81.0 into a separate directory and apply TMS9900 patches (calling convention, LLVM component registration)
2. Build stage 1: `python3 x.py build --stage 1 library`
3. Register toolchain: `rustup toolchain link tms9900 build/<host>/stage1`

**Build a Rust program**:

```bash
cargo +tms9900 build -Z build-std=core --target tms9900-unknown-none.json
```

See the [rust-tms9900](https://github.com/apullin/rust-tms9900) repository for the patched Rust fork, target spec, and examples.

**Note**: `core::arch::asm!` is not available for custom targets. Use `extern "C"` calls to assembly files instead.

## Runtime Libraries (`libtms9900/`)

### Compiler Builtins (`builtins/`)

Hand-coded TMS9900 assembly for performance-critical operations:

| Category | Functions |
|----------|-----------|
| 32-bit integer | `__mulsi3`, `__divsi3`, `__udivsi3`, `__modsi3`, `__umodsi3` |
| 32-bit shifts | `__ashlsi3`, `__lshrsi3`, `__ashrsi3` |
| 64-bit integer | `__muldi3`, `__divdi3`, `__udivdi3`, `__moddi3`, `__umoddi3`, `__udivmoddi4` |
| Memory | `memcpy`, `memset`, `memmove` (auto-increment optimized) |
| Soft-float | `__addsf3`, `__subsf3`, `__mulsf3`, `__divsf3`, comparisons, conversions |

The 32-bit division includes a hardware `DIV` fast path for 16-bit divisors. Memory routines exploit the TMS9900's `*R+` auto-increment addressing mode.

### Math Library (`libm/`)

Single-precision math built from picolibc sources (~21KB at `-Os`):

`sinf`, `cosf`, `tanf`, `asinf`, `acosf`, `atanf`, `atan2f`, `sinhf`, `coshf`, `tanhf`, `expf`, `logf`, `log10f`, `log2f`, `powf`, `sqrtf`, `fabsf`, `fmodf`, `ceilf`, `floorf`, `roundf`, `scalbnf`, `copysignf`, `ldexpf`, `frexpf`

Custom compact `sinf`/`cosf` implementation: 1.2KB combined (vs 7KB+ from stock picolibc).

## TMS9900 Architecture

The TMS9900 is a 16-bit big-endian microprocessor with a unique workspace-pointer architecture:

- **16 "registers"** are actually words in RAM, pointed to by the Workspace Pointer (WP)
- **64KB address space**, 16-bit data and instruction words
- **No hardware stack** — software stack via R10
- **Hardware multiply** (`MPY`: 16×16→32) and **divide** (`DIV`: 32÷16→16)
- **Auto-increment addressing**: `MOV *R1+, R3` (source operand only)

### Calling Convention

| Register | Usage |
|----------|-------|
| R0       | Return value (16-bit), or R0:R1 (32-bit high:low) |
| R1–R9    | Arguments (first 9 words), caller-saved |
| R10      | Stack Pointer |
| R11      | Link Register (return address) |
| R12      | CRU Base / general purpose |
| R13–R15  | Callee-saved |

Arguments: R1–R9, then stack. Stack grows downward, 4-byte aligned. 32-bit arguments use register pairs (R1:R2, R3:R4, ...).

## Testing

### Benchmark Suite (20 programs)

All 60/60 pass (20 programs × 3 optimization levels):

| Benchmark | Description | Code (O2) | Cycles (O2) |
|-----------|-------------|-----------|-------------|
| fib | Fibonacci | 282B | 1.5K |
| bubble_sort | Array sort | 392B | 21.6K |
| crc32 | CRC-32 hash | 418B | 77.5K |
| json_parse | JSON tokenizer | 978B | 20.4K |
| string_torture | String operations | 502B | 11.5K |
| float_torture | IEEE 754 soft-float | 8.6KB | 1.7K |
| huffman | Huffman codec | 1.3KB | 523K |
| long_torture | 30 tests of 32-bit ops | 1.8KB | 19.8K |
| heap4 | FreeRTOS-style allocator | 2.1KB | 121K |
| i64_torture | 64-bit arithmetic | 7.1KB | 349K |
| cpp_test | Ctors, vtables, templates | 510B | 3.0K |
| lambda_test | Lambda expressions | 2.9KB | 4.0K |
| mi_test | Multiple inheritance | 1.7KB | 9.6K |
| stl_test | vector, string | 15.4KB | 30.8K |
| stl_util_test | tuple, optional, unique_ptr, ... | 1.5KB | 9.1K |
| *+ 5 more* | | | |

### Stress Tests

| Test | Status | Notes |
|------|--------|-------|
| **Csmith** (100 programs) | 100/100 self-consistent | O0=O1=O2 where all complete |
| **CoreMark** | PASS at O0–Os | ~1.3M cycles at O2 |
| **MiniLZO** | PASS at O1–Os | Compression + decompression |
| **sprintf** (35 tests) | 35/35 pass O0–O2 | Variadic functions, formatting |

### LLVM Lit Tests

98 tests (82 CodeGen + 16 MC), all passing.

## Assembler

The integrated assembler supports both LLVM and xas99 syntax:

| Feature | LLVM Style | xas99 Style |
|---------|------------|-------------|
| Labels | `LABEL:` | `LABEL` (no colon) |
| Hex | `0x1234` | `>1234` |
| Comments | `#`, `//`, `;` | `;` |

**xas99 directives**: `DATA`, `BYTE`, `TEXT`, `BSS`, `DEF`, `REF`, `EQU`, `END`.

**CRU bit-addressing**: `SBO`, `SBZ`, `TB` with symbolic offsets.

## TI-99/4A Cartridge Examples

The `cart_example/` directory has several demos:

- **life2x.c** — Game of Life with 2× pixels, keyboard controls, dirty-tile tracing
- **ball.c / ball2.c** — Bouncing ball demos
- **banner.c** — Text banner

```bash
cd cart_example
make life2x.img    # EA5 executable
make banner.rpk    # MAME cartridge
```

## Exotic TODOs

- **Workspace-aware compilation** — The TMS9900's workspace pointer architecture means a context switch (`BLWP`) swaps all 16 registers atomically via a pointer change. A workspace-aware compiler could allocate "hot" variables directly in the workspace for ISR-heavy or coroutine-style code, avoiding save/restore overhead entirely.
- **Debug info / DWARF** — Not yet implemented.
- **CRU-aware register allocation** — Reserve R12 only when CRU I/O instructions are used (currently opt-in via `-mattr=+reserve-cru`).

## Resources

- [tms9900-trace](https://github.com/apullin/tms9900-trace) — TMS9900 CPU emulator for testing
- [xdt99](https://github.com/endlos99/xdt99) — Cross-development tools for TI-99/4A
- [PROJECT_JOURNAL.md](PROJECT_JOURNAL.md) — Detailed development log

## License

Apache 2.0 with LLVM Exceptions (follows upstream LLVM licensing).
