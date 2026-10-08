# Runtime and startup regressions

These are optional execution tests for the tools repository. LLVM's in-tree
TMS9900 CodeGen and MC tests do not require a simulator.

Build the Release TMS9900 toolchain and `tms9900-trace`, then run from the
repository root:

```sh
python3 tests/runtime/run_runtime_tests.py \
  --trace ../tms9900-trace/build/tms9900-trace
```

The runner compiles and executes independent programs at `-O0`, `-O1`, `-O2`,
`-Os`, and `-Oz`. Use `--opt 3` to test `-O3`, `--filter crt` (or `memset`,
`malloc`, `peephole`) to narrow the run, and `--llvm-bin PATH` to select a build.
Temporary objects and images are removed automatically.

- **Peepholes:** exact post-RA MIR kernels check both signed byte comparisons,
  a masked address alias, a byte store followed by a full-word return, shift
  carry, and ABS result comparisons. The C caller checks all 256 byte values
  and all 65,536 word values. `--llc PATH` permits comparison with an older
  backend while retaining the current C caller.
- **memset:** every 16-bit fill argument, plus zero/odd/even lengths and both
  destination alignments, checks the returned pointer and untouched guards.
- **malloc:** oversized and wrapping requests, odd-sized allocations, a failed
  allocation followed by a valid one, and exhaustion are tested with aligned,
  odd, and near-address-space-end heap bounds.
- **Startup:** the actual cartridge and EA5 scripts/startup initialize empty,
  one-, two-, and three-byte sections, including an odd ROM data source. BSS is
  prefilled with nonzero bytes, a canary after BSS must survive, and a C leaf
  checks stack alignment and an i32 stack round trip. Link checks reject static
  storage overlapping the reserved stack and verify a configurable stack/heap
  boundary in both maps.

The demo maps reserve 2 KiB at the top of expansion RAM for the C stack. Use
`ld.lld --defsym=__stack_size=0x1000 -T SCRIPT ...` to reserve 4 KiB instead
(define it before reading the script). They provide
`__heap_start`/`__heap_end` for malloc and `__stack_top` for startup. This prevents
static-storage/heap overlap with the reservation; runtime stack exhaustion
still requires choosing a sufficient stack size for the program.

The startup fixture includes a dummy cartridge header so objcopy's binary
starts at `>6000`. Extending the simulator image to prefill BSS represents its
initial RAM contents; that extended image is not a cartridge distribution file.
Tests halt with `IDLE`, returning zero in R0 on success and a numbered failure
in R0 otherwise. No VDP, keyboard, or TI console ROM emulation is needed.

`run_demo_tests.py --trace PATH` uses the actual demo Makefile and stops just
before main in both banner programs. At all six optimization levels it checks
the workspace, masked interrupts, and aligned expansion-RAM stack in cartridge
and EA5 builds. It runs from a temporary copy and leaves local demo images alone.
Add `--build-all` to build all eleven demos in every distribution format at
default -O3, verifying ROM bounds, identical BIN/CART/RPK payloads and EA5 headers.
`test_exports.py` separately checks converter metadata, malformed/range inputs,
and PCB choices without requiring the compiler or simulator.

`run_build_tests.py` checks configuration and header dependencies. Its host
fixture covers partial flag changes, compiler replacement, parallel builds,
spaces/quotes in settings, and read-only `make -n`. It also switches real
builtins/libm builds from -O0 to -O2 through a partial object request, then
compares each completed archive byte-for-byte with a fresh -O2 build. Use
`--host-only` to run without the TMS9900 toolchain.
The target run also verifies demo partial flag switches and a changed stack
reservation, including invalidation of unrequested demo images.

`run_trig_tests.py` builds libm and its real soft-float runtime at five
optimization levels. It checks sine/cosine on 244 binary32 inputs: signed zero,
subnormals, quadrant boundaries, large finite values, infinities, signaling and
quiet NaNs, deterministic arbitrary bit patterns and embedded-scale angles.
It also checks binary32 classification and signaling-NaN/signbit dispatch in
the actual picolibc headers, plus roundf ties and signed zero. Each library
build starts with a stale override archive member to verify upgrade cleanup.
Reference
values come from the host's double-precision libm, rounded to binary32, with a
two-ULP finite-value tolerance; this is not an exhaustive or formal accuracy
proof. Stored results are checked independently from a simulator RAM dump as
well as by the compiled C test. No test-local math-error helper stubs are linked.
