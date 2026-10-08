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
  checks stack alignment and an i32 stack round trip.

The startup fixture includes a dummy cartridge header so objcopy's binary
starts at `>6000`. Extending the simulator image to prefill BSS represents its
initial RAM contents; that extended image is not a cartridge distribution file.
Tests halt with `IDLE`, returning zero in R0 on success and a numbered failure
in R0 otherwise. No VDP, keyboard, or TI console ROM emulation is needed.
