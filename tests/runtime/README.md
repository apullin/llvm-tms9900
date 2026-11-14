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
Add `--build-all` to build all eleven demos in every distribution format with
their default flags (-O2 for Life3, -O3 for the others), verifying ROM bounds,
identical BIN/CART/RPK payloads and EA5 headers.
`test_exports.py` separately checks converter metadata, malformed/range inputs,
and PCB choices without requiring the compiler or simulator.

`run_life_tests.py --trace PATH` executes all three scaled demos' actual seeding
functions at all six optimization levels. It checks wrapped blocks at six
origins, guards on both sides of the board, stack/register preservation and
8,192 bounded RNG samples covering all 96 rows. Life3's assembly is tested on
all 512 nine-cell neighborhoods at every horizontal position (65,536 focused
checks), comparing entire output rows against independent scalar Life rules.
The actual Life3 C/assembly update then runs 64 generations on sparse wrapping,
deterministic random and empty boards, comparing final board bytes, exact dirty
flags and next activity against the scalar reference. The unused destination
starts with stale nonzero bytes to exercise inactive-row clearing.

Rendering tests verify every emitted VDP address/data byte for all 256 source
byte values, including partial dirty rows and completely clean rows. A keyboard
stub returning F (after SPACE for Life3's splash) exercises the actual main-loop
snapshot lifecycle without TI console ROM emulation. Invalid Life3
width/height/scale settings must fail at compile time. Use
`--filter seed row frames render snapshot geometry startup splash perturb` to select
parts and `--opt 2` for a narrower run. No simulator dependency is added to LLVM.

The splash checks reconstruct Graphics I VRAM from the actual port writes:
title, version/date, controls, font bytes, colors and hidden sprites. Scripted
keyboard input proves that no key and F stay on the splash, SPACE starts Life,
and the display is blanked before changing to Graphics II. The shipping
keyboard helper remains in the measured image; only the CPU test redirects
its TI console ROM call boundary. Add `--splash-preview /tmp/life-splash.png`
to render the captured VRAM. The tests first link Life3 with the cartridge map
and its physical 8 KiB limit. If that specific bound is exceeded (currently
`-O0` and `-O3`), they use the shipped EA5 map. Other link errors fail the test.
Distributed cartridge images are checked separately with the demo defaults.

The `perturb` checks exercise the N/M helpers on empty, sparse and fully live
boards with multiple RNG seeds, covering every disk radius from 12 through 23,
wrapping left/right/top/bottom and corners, and interior placement.
An independent cell-coordinate model checks exact additions and final
RNG state, preserved live cells, existing dirty flags, next activity, buffer
guards and the ABI. Actual-main keyboard sequences check uppercase/lowercase
dispatch, no repetition while held, release and a second press. Interactive
random byte samples advance the LFSR eight steps to avoid overlapping the
seven low bits of consecutive states; the original startup seed is retained.

For a before/after timing comparison, save the old `life3_2x.elf` and `.bin`
before rebuilding, then run:

```sh
python3 tests/runtime/run_life_tests.py --trace PATH --opt 2 \
  --baseline-dir /path/to/saved/images --results /tmp/life-cycles.json
```

Use `--baseline-opt 2` when the saved image was built at `-O2` (default label
is `-O3`). Saved images with a splash use the test keyboard boundary to reach
their initial board before comparing compute/draw intervals.

Both builds receive the same boards, including the saved cart's initial seed.
Tracepoints pair `step_board`/`life_step_end` and
`vdp_write_dirty_patterns`/`life_draw_end`. The begin labels inside inline asm
may become repeated loop locations; use function entry symbols instead.
Results contain mean and population standard deviation in estimated zero-wait
CPU cycles. Intervals exclude function epilogues, main-loop glue, keyboard,
startup and reseeding; the CPU simulator does not emulate the VDP/display.
The `startup` checks also execute each scaled demo's actual CRT and main up to
the first update, comparing the seeded board, final RNG state and initial
activity with a host reference. The other board and a guard after BSS must
remain untouched. These checks cover inlined seeding in the real demo build.

`run_lto_tests.py --trace PATH` executes full TU-aware LTO at all six optimization
levels and both input orders. Two units share static names, and one static name
also collides with an external definition. An alwaysinline callee belongs to a
different script region and must remain there. Native placement, independently
computed return values, the restored stack and preserved registers are checked
for five boundary inputs per build. This is a CPU test, without TI console ROM
or VDP dependencies. Use `-flto=full` for TU-aware linker-script mode; ThinLTO
inputs are explicitly rejected because equivalent KEEP/placement/import/cache
support is not implemented.

`run_build_tests.py` checks configuration and header dependencies. Its host
fixture covers partial flag changes, compiler replacement, parallel builds,
spaces/quotes in settings, and read-only `make -n`. It also switches real
builtins/libm builds from -O0 to -O2 through a partial object request, then
compares each completed archive byte-for-byte with a fresh -O2 build. Use
`--host-only` to run without the TMS9900 toolchain.
The target run also verifies demo partial flag switches and a changed stack
reservation, including invalidation of unrequested demo images. Life3's default
-O2, its `LIFE3_OPT` setting and command-line `CFLAGS` precedence are checked
against independently compiled objects; an unchanged build preserves timestamps.

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
