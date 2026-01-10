# libtms9900 - Runtime Library for TMS9900 LLVM Backend

Runtime libraries providing compiler builtins and math functions for the TMS9900 LLVM backend.

## Directory Structure

```
libtms9900/
├── builtins/          # Compiler intrinsics (__mulsi3, __addsf3, etc.)
│   ├── mul32.asm      # 32-bit multiply
│   ├── div32.asm      # 32-bit divide/modulo
│   ├── shift32.asm    # 32-bit shifts
│   └── softfloat/     # IEEE 754 soft-float (from compiler-rt)
├── libm/              # Math library
│   ├── sincosf_tiny.c # Compact sin/cos (our implementation)
│   ├── include/       # Configuration headers
│   └── Makefile       # Builds libm.a
└── picolibc/          # Embedded C library (upstream source)
```

**TODO**: Convert picolibc to a git submodule pointing to https://github.com/apullin/picolibc

## Building

### libm.a (Math Library)

```bash
cd libm
make          # Build with -Os (default, smallest)
make clean
```

To build with different optimization, edit CFLAGS in the Makefile.

**Note**: -O0 currently fails due to branch relaxation issues in the compiler.

## Calling Convention

All functions follow the TMS9900 LLVM calling convention:
- Arguments in R0, R1, R2, R3 (first 4 words)
- 32-bit values: high word first (R0:R1 or R2:R3)
- Return in R0 (16-bit) or R0:R1 (32-bit)
- R11 = link register
- Callee-saved: R9, R10 (R10 = stack pointer)

## 32-bit Integer Builtins

| Symbol | Purpose |
|--------|---------|
| `__mulsi3` | 32-bit multiply |
| `__divsi3` | 32-bit signed divide |
| `__udivsi3` | 32-bit unsigned divide |
| `__modsi3` | 32-bit signed modulo |
| `__umodsi3` | 32-bit unsigned modulo |
| `__ashlsi3` | 32-bit left shift |
| `__ashrsi3` | 32-bit arithmetic right shift |
| `__lshrsi3` | 32-bit logical right shift |

## Soft-Float Builtins

IEEE 754 single-precision floating-point operations, provided by LLVM's compiler-rt.

Located in `builtins/softfloat/`.

| Symbol | Purpose |
|--------|---------|
| `__addsf3` | float32 add |
| `__subsf3` | float32 subtract |
| `__mulsf3` | float32 multiply |
| `__divsf3` | float32 divide |
| `__eqsf2`, `__nesf2` | float32 equality |
| `__ltsf2`, `__lesf2` | float32 less-than |
| `__gtsf2`, `__gesf2` | float32 greater-than |
| `__fixsfsi` | float32 → int32 |
| `__floatsisf` | int32 → float32 |

## Picolibc Integration

The `picolibc/` directory contains upstream [picolibc](https://github.com/picolibc/picolibc) source. We currently use only the libm components, building selected files via `libm/Makefile`. This will likely expand to include libc components as the project matures.

## Compact sinf/cosf Implementation

We provide a size-optimized `sinf`/`cosf` that replaces picolibc's 7KB+ implementation with a 1.2KB version.

### How It Works

Standard libm sin/cos implementations use Payne-Hanek range reduction, which requires a ~200 byte table of 2/π digits and complex multi-precision arithmetic to handle `sin(1e10)` accurately. This pulls in 8KB+ of code.

Our implementation uses Cody-Waite style range reduction with extended-precision π/2 constants:
- `pio2_hi` = high part of π/2
- `pio2_lo` = low part (correction term)

### Tradeoffs

| Property | Our Implementation | Full picolibc |
|----------|-------------------|---------------|
| Size | 1.2KB | 7KB+ |
| Accuracy for \|x\| < 10^4 | Full precision | Full precision |
| Accuracy for \|x\| > 10^6 | Reduced (~4 digits) | Full precision |
| ULP error (small x) | < 1 ULP | < 1 ULP |

**When to use full picolibc sin/cos**: If you need `sin(1e10)` to be accurate to full float precision, rebuild with picolibc's sf_sin.c/sf_cos.c instead of sincosf_tiny.c.

**For typical embedded use** (angles in radians, game physics, signal processing), our compact version is accurate and 6x smaller.

## libm Symbol Table

All sizes in bytes. **Note**: These sizes will change as the compiler improves.

| Symbol | Function | -O1 | -Os | -O2 | -O3 |
|--------|----------|----:|----:|----:|----:|
| `acoshf` | Inverse hyperbolic cosine | 364 | 360 | 358 | 358 |
| `acosf` | Inverse cosine | 1,462 | 1,462 | 1,462 | 1,472 |
| `asinhf` | Inverse hyperbolic sine | 588 | 580 | 588 | 648 |
| `asinf` | Inverse sine | 1,428 | 1,420 | 1,428 | 1,482 |
| `atan2f` | Two-argument arctangent | 718 | 704 | 718 | 740 |
| `atanf` | Arctangent | 998 | 990 | 998 | 1,002 |
| `atanhf` | Inverse hyperbolic tangent | 336 | 346 | 336 | 342 |
| `ceilf` | Ceiling | 352 | 350 | 352 | 368 |
| `copysignf` | Copy sign | 14 | 14 | 14 | 14 |
| `coshf` | Hyperbolic cosine | 450 | 458 | 450 | 480 |
| `cosf` | Cosine (compact impl) | *incl.* | *incl.* | *incl.* | *incl.* |
| `expf` | Exponential | 1,208 | 1,216 | 1,208 | 1,254 |
| `exp2f` | Base-2 exponential (wrapper) | 22 | 22 | 22 | 22 |
| `fabsf` | Absolute value | 6 | 6 | 6 | 6 |
| `finitef` | Finite check | 26 | 26 | 26 | 32 |
| `floorf` | Floor | 314 | 306 | 314 | 314 |
| `fmodf` | Floating-point modulo | 1,254 | 1,168 | 1,230 | 1,240 |
| `frexpf` | Extract mantissa/exponent | 158 | 146 | 154 | 164 |
| `hypotf` | Hypotenuse | 1,256 | 1,238 | 1,256 | 1,318 |
| `isnanf` | NaN check | 62 | 58 | 62 | 62 |
| `log10f` | Base-10 logarithm | 326 | 326 | 318 | 330 |
| `log2f` | Base-2 logarithm (wrapper) | 24 | 24 | 24 | 24 |
| `logf` | Natural logarithm | 1,436 | 1,404 | 1,428 | 1,448 |
| `modff` | Integer/fraction split | 244 | 264 | 244 | 244 |
| `powf` | Power function | 5,312 | 5,100 | 5,326 | 5,428 |
| `scalbnf` | Scale by power of 2 | 398 | 350 | 376 | 402 |
| `sinf` | Sine (compact impl) | *incl.* | *incl.* | *incl.* | *incl.* |
| `sinhf` | Hyperbolic sine | 430 | 424 | 430 | 456 |
| `sqrtf` | Square root | 740 | 736 | 740 | **4,416** |
| `tanhf` | Hyperbolic tangent | 282 | 282 | 282 | 294 |
| **sincosf_tiny.o** | sinf + cosf combined | 1,314 | **1,200** | 1,396 | 1,456 |
| **TOTAL** | | 21,522 | **20,980** | 21,546 | 25,816 |

*Note: sinf and cosf are both in sincosf_tiny.o; linker pulls the whole object if either is used.*

### Observations

- -Os is the clear winner for size at 20,980 bytes
- -O1, -O2 are nearly identical (~21.5KB) - only ~500 bytes apart
- -O3 explodes to 25.8KB due to aggressive inlining (sqrtf alone: 4.4KB vs 736B)
- Speed is critical on TMS9900; -O3 may be worthwhile once we have cycle-count benchmarks
- powf is surprisingly large at 5.1KB (24% of total library size)

**TODO**: Benchmark -Os vs -O2 vs -O3 for runtime performance. The eventual cycle-count scheduler (with memory latency modeling) will help determine the best tradeoff.

**TODO**: Investigate powf size/performance. Proposed improvements:
- Special-case small integer exponents (|y| ≤ 10) with multiply loop (faster AND more precise)
- Handle negative small integers as 1/(x^n)
- Benchmark current implementation to determine if 126 soft-float calls are acceptable

## License

Part of the llvm-99 project. See main project LICENSE.
picolibc is BSD-licensed; see picolibc/COPYING.picolibc.
