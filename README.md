# LLVM Backend for TMS9900 CPU

A production-quality LLVM backend for the [Texas Instruments TMS9900](https://en.wikipedia.org/wiki/TMS9900) — a unique and long-forgotten 16-bit processor that was surprisingly modern in its architecture, but famously hamstrung in the ill-fated [TI-99/4A](https://en.wikipedia.org/wiki/TI-99/4A) home computer. Compiles C, C++ (with STL), and Rust 🦀 to native TMS9900 machine code.

## Highlights

- **Full LLVM 18 toolchain** — clang, opt, lld, llvm-objcopy, assembler, and disassembler. No external tools required.
- **Heavily stress-tested** — 100 Csmith random programs, CoreMark, MiniLZO, sprintf test suite, and 20 hand-written benchmarks (60/60 pass across -O0, -O1, -O2, and -Os).
- **C++ with STL** — freestanding libc++ headers: `vector`, `string`, `tuple`, `optional`, `unique_ptr`, `bitset`, algorithms, and more. Lambdas, multiple inheritance, move semantics, variadic templates all working.
- **Rust `#![no_std]`** 🦀 — builds with `cargo +tms9900 build -Z build-std=core`. See [Rust Support](#rust-support) below.
- **FreeRTOS port** — preemptive multitasking with near-zero-cost context switches via the TMS9900's workspace pointer. See [FreeRTOS](#freertos).
- **Hand-tuned runtime library** — assembly-optimized 32-bit multiply/divide/shift, 64-bit arithmetic, `memcpy`/`memset`/`memmove` exploiting auto-increment addressing, and IEEE 754 soft-float.
- **picolibc math** — single-precision `libm` (sin, cos, sqrt, exp, log, pow, ...) built from [picolibc](https://github.com/picolibc/picolibc) sources, with a custom compact sinf/cosf (1.2KB vs 7KB+ standard).
- **GDB/LLDB debugging** — the [tms9900-trace](https://github.com/apullin/tms9900-trace) emulator includes a GDB Remote Serial Protocol stub for interactive debugging with breakpoints, single-stepping, and memory inspection.

## The CPU

The [TMS9900](https://en.wikipedia.org/wiki/TMS9900) (1976) was TI's flagship 16-bit microprocessor. Its most distinctive feature is the *workspace pointer* architecture: instead of fixed hardware registers, the CPU's 16 "registers" are just 32 bytes of RAM pointed to by the Workspace Pointer (WP). A context switch is a single instruction (`BLWP`) that atomically swaps all 16 registers by changing one pointer — no pushing, no popping, no saving.

The chip also has hardware multiply (16×16→32) and divide (32÷16→16), auto-increment addressing, and a bit-addressable I/O bus (CRU). It was genuinely ahead of its time.

For full details, see the [TMS 9900 Microprocessor Data Manual](https://archive.org/details/tms-9900-microprocessor-data-manual-may-76) (1976).

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

## Repository Structure

```
llvm-tms9900/
├── llvm-project/                # LLVM 18 fork (submodule, branch: tms9900)
├── libtms9900/
│   ├── builtins/                # Runtime intrinsics (32/64-bit ops, memcpy, soft-float)
│   ├── libm/                    # Math library (picolibc-based, float32)
│   └── picolibc/                # Embedded C library source
├── FreeRTOS/                    # FreeRTOS port with TMS9900 workspace-based context switch
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

## FreeRTOS

The `FreeRTOS/` directory contains a full FreeRTOS port that exploits the TMS9900's workspace pointer for near-zero-cost context switching.

### Why the TMS9900 is perfect for an RTOS

On most CPUs, a context switch means saving and restoring all registers — typically dozens of push/pop instructions. On the TMS9900, each task owns a permanent 32-byte *workspace* in RAM. Switching tasks means changing a single 16-bit pointer (WP). The entire register file swaps atomically — no copying, no stack gymnastics.

The FreeRTOS port stores each task's workspace at the top of its stack allocation. On context switch, only 4 words are saved (WP, PC, ST, critical nesting counter), then the new task's WP is loaded and execution resumes with all 16 registers already in place.

### Demo programs

**Basic two-task test** (`main.c`): Two equal-priority tasks increment separate memory markers under preemptive time-slicing. Verifies that the scheduler correctly alternates between tasks.

**Queue + event group rendezvous** (`main_queue.c`): A port of an [MSPM0 Rust demo](https://github.com/apullin/freertos-in-rust-mspm0-demo). A manager task generates random work items and pushes them to a queue; three worker tasks consume items and synchronize at a 4-way event-group rendezvous barrier. Exercises queues, event groups, multiple priority levels, and tick-based delays.

```bash
cd FreeRTOS
make run          # basic two-task test
make run-queue    # queue + rendezvous test
```

## Debugging and Emulation

Development and testing uses [tms9900-trace](https://github.com/apullin/tms9900-trace), a standalone TMS9900 CPU emulator. It was invaluable during compiler development — every backend bug was caught and diagnosed using its cycle-accurate execution traces, memory dumps, and JSON output mode.

### GDB/LLDB remote debugging

The emulator includes a full GDB Remote Serial Protocol (RSP) stub. Connect with GDB or LLDB for interactive debugging:

```bash
# Start emulator in GDB server mode
tms9900-trace -l 0x0000 program.bin --gdb

# In another terminal
lldb
(lldb) gdb-remote localhost:1234
(lldb) register read
(lldb) breakpoint set -a 0x0100
(lldb) continue
```

Supports breakpoints, single-stepping, register and memory inspection, and async break (Ctrl-C).

### Tracepoints

Lightweight PC-hit profiling without full instruction trace overhead:

```bash
tms9900-trace -l 0x0000 program.bin --tracepoint 0x0100 --tracepoint 0x0200 -n 500000 -S
```

Reports hit counts, first/last cycle timestamps — useful for measuring function call frequency and loop iteration counts.

### DWARF debug info

DWARF emission is not yet implemented. ELF binaries contain symbol tables (function names, globals) which are sufficient for the GDB stub's breakpoint and symbol lookup features. Full source-level debugging with line numbers and variable inspection is a future goal.

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

## Calling Convention

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

### Benchmark Suite

20 programs (14 C + 6 C++), all passing at every optimization level:

| Benchmark | Description | -O0 | -O1 | -O2 | -O0 | -O1 | -O2 |
|-----------|-------------|----:|----:|----:|----:|----:|----:|
| | | Code | Code | Code | Cycles | Cycles | Cycles |
| fib | Fibonacci | 184 | 108 | 282 | 7.1K | 2.2K | 1.5K |
| bubble_sort | Array sort | 440 | 194 | 392 | 117K | 26.9K | 21.6K |
| deep_recursion | Tail-call recursion | 218 | 108 | 144 | 45.6K | 6.3K | 766 |
| crc32 | CRC-32 hash | 514 | 220 | 418 | 204K | 111K | 77.5K |
| q7_8_matmul | Fixed-point matrix multiply | 392 | 134 | 134 | 9.9K | 512 | 512 |
| json_parse | JSON tokenizer | 2.7K | 1.0K | 978 | 115K | 22.5K | 20.4K |
| string_torture | String operations | 740 | 500 | 502 | 34.0K | 12.3K | 11.5K |
| float_torture | IEEE 754 soft-float | 11.4K | 8.6K | 8.6K | 48.0K | 1.7K | 1.7K |
| bitops_torture | popcount, bswap, clz, ctz | 3.3K | 2.1K | 2.1K | 72.5K | 27.1K | 27.1K |
| vertex3d | 3D vertex transforms | 1.1K | 424 | 852 | 95.0K | 57.2K | 56.0K |
| huffman | Huffman codec | 3.6K | 1.4K | 1.3K | 1.86M | 554K | 523K |
| long_torture | 30 tests of 32-bit ops | 3.3K | 1.8K | 1.8K | 41.2K | 19.8K | 19.8K |
| heap4 | FreeRTOS-style allocator | 4.2K | 1.8K | 2.1K | 401K | 125K | 121K |
| i64_torture | 64-bit arithmetic | 9.1K | 7.1K | 7.1K | 374K | 349K | 349K |
| cpp_test | Ctors, vtables, templates | 2.1K | 510 | 510 | 15.5K | 3.0K | 3.0K |
| lambda_test | Lambda expressions | 18.9K | 3.1K | 2.9K | 66.5K | 5.9K | 4.0K |
| mi_test | Multiple inheritance | 7.5K | 1.9K | 1.7K | 63.7K | 9.8K | 9.6K |
| cpp_adv_test | Move, forwarding, variadic | 4.8K | 1.2K | 1.1K | 92.4K | 16.9K | 17.8K |
| stl_test | vector, string | 54.1K | 16.5K | 15.4K | 716K | 34.2K | 30.8K |
| stl_util_test | tuple, optional, unique_ptr | 12.0K | 1.6K | 1.5K | 188K | 10.5K | 9.1K |

Code sizes in bytes. Cycle counts from tms9900-trace emulator.

### Stress Tests

| Test | Result | Notes |
|------|--------|-------|
| **Csmith** (100 programs) | 100/100 self-consistent | O0=O1=O2 where all complete |
| **CoreMark** | O0–Os | ~1.3M cycles at O2 |
| **MiniLZO** | O1–Os | Lossless compression + decompression round-trip |
| **sprintf** (35 tests) | 35/35 at O0–O2 | Variadic functions, format strings |

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

- **Workspace-aware compilation** — The TMS9900's workspace pointer architecture means a context switch (`BLWP`) swaps all 16 registers atomically by changing one pointer. A workspace-aware compiler could allocate "hot" variables directly in the workspace for ISR-heavy or coroutine-style code, avoiding save/restore overhead entirely. This is architecturally unique to the TMS9900 family and has no equivalent in modern compiler backends.
- **Migrate to LLVM latest** — The backend currently targets LLVM 18. Porting to the LLVM main branch would unlock newer optimization passes and keep pace with upstream improvements.
- **Full DWARF debug info** — Source-level debugging with line numbers and variable inspection.
- **CRU-aware register allocation** — Reserve R12 only when CRU I/O instructions are used (currently opt-in via `-mattr=+reserve-cru`).

## Resources

- [TMS 9900 Microprocessor Data Manual](https://archive.org/details/tms-9900-microprocessor-data-manual-may-76) (1976) — the definitive reference
- [tms9900-trace](https://github.com/apullin/tms9900-trace) — TMS9900 CPU emulator with GDB stub
- [xdt99](https://github.com/endlos99/xdt99) — Cross-development tools for TI-99/4A
- [PROJECT_JOURNAL.md](PROJECT_JOURNAL.md) — Detailed development log

## License

Apache 2.0 with LLVM Exceptions (follows upstream LLVM licensing).
