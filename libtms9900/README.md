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

## sinf/cosf correctness

The default build uses picolibc's `sf_sin.c`/`sf_cos.c`, compensated polynomial
kernels, and full-range binary32 argument reduction. This shares the reducer
already used by `tanf`. Huge finite arguments, infinities, NaNs, signed zero,
and subnormals are part of the supported domain. This build offers IEEE results
without errno handling, as configured in `libm/include/picolibc.h`.

The former 1.2 KiB compact override has been removed. It lost thousands of ULPs
at 10,000 radians, overflowed the TMS9900's 16-bit quadrant counter for larger
inputs, and mishandled infinities/NaNs. Even near pi/4 its sine and cosine
polynomials missed the previously advertised one-ULP accuracy.

Full-range reduction costs more code than that approximation; a future small
angle library should expose its input domain and error bound through a separate
API. Run the optional execution regression with:

```sh
python3 tests/runtime/run_trig_tests.py --trace /path/to/tms9900-trace
```

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
| `cosf` | Cosine (full-range implementation; sizes pending) | — | — | — | — |
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
| `sinf` | Sine (full-range implementation; sizes pending) | — | — | — | — |
| `sinhf` | Hyperbolic sine | 430 | 424 | 430 | 456 |
| `sqrtf` | Square root | 740 | 736 | 740 | **4,416** |
| `tanhf` | Hyperbolic tangent | 282 | 282 | 282 | 294 |
| **TOTAL** | | 21,522 | **20,980** | 21,546 | 25,816 |

These object-size measurements are historical. The sine/cosine override was
removed during the correctness audit; its measurements do not describe the
current library. Use `make sizes` with `llvm-size` available for current sizes.

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
