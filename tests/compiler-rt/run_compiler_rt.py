#!/usr/bin/env python3
"""
Compiler-RT Builtin Test Runner for TMS9900

Compiles and runs compiler-rt builtin unit tests through the tms9900-trace
emulator. Tests pass if they halt via IDLE with R0=0.

Usage:
    python3 run_compiler_rt.py                    # Run all, -O2
    python3 run_compiler_rt.py --opt 0            # Run all, -O0
    python3 run_compiler_rt.py --filter mulsi3    # Run matching tests
    python3 run_compiler_rt.py --verbose           # Show compile commands
    python3 run_compiler_rt.py --compile-only      # Just check compilation
"""
import argparse
import json
import os
import pathlib
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[2]
LLVM_BIN = ROOT / "llvm-project" / "build" / "bin"
CLANG = LLVM_BIN / "clang"
LLD = LLVM_BIN / "ld.lld"
OBJCOPY = LLVM_BIN / "llvm-objcopy"
HOME = pathlib.Path.home()
TMS9900_TRACE = os.environ.get(
    "TMS9900_TRACE",
    str(HOME / "personal" / "ti99" / "tms9900-trace" / "build" / "tms9900-trace"),
)

CRT_TEST_DIR = ROOT / "llvm-project" / "compiler-rt" / "test" / "builtins" / "Unit"
CRT_BUILTINS_DIR = ROOT / "llvm-project" / "compiler-rt" / "lib" / "builtins"
SHIM_DIR = pathlib.Path(__file__).resolve().parent
INCLUDE_DIR = SHIM_DIR / "include"
BENCH_LD = ROOT / "tests" / "benchmarks" / "bench.ld"
BUILTINS = ROOT / "libtms9900" / "builtins" / "build" / "libbuiltins.a"

# Softfloat sources (compiled separately, linked for float-using tests)
SOFTFLOAT_DIR = ROOT / "libtms9900" / "builtins" / "softfloat"
SOFTFLOAT_SRCS = [
    "addsf3.c", "subsf3.c", "mulsf3.c", "divsf3.c", "negsf2.c",
    "comparesf2.c", "floatsisf.c", "floatunsisf.c", "fixsfsi.c",
    "fixunssfsi.c", "clzsi2.c",
]
FP_BUILTINS_SRC = ROOT / "tests" / "fp32_builtins.c"

MAX_STEPS = 50_000_000

# Common CFLAGS
CFLAGS_BASE = [
    "--target=tms9900",
    "-ffreestanding",
    "-fno-builtin",
    f"-I{INCLUDE_DIR}",
    f"-I{CRT_BUILTINS_DIR}",
    "-Wno-implicit-int",
    "-Wno-implicit-function-declaration",
]

# ============================================================================
# Tests to skip: these cannot work on TMS9900 for legitimate reasons
# ============================================================================
SKIP_TESTS = set()

# --- 128-bit integer (no __int128 on 16-bit target) ---
SKIP_TESTS |= {
    "absvti2_test.c", "addvti3_test.c", "ashlti3_test.c", "ashrti3_test.c",
    "clzti2_test.c", "cmpti2_test.c", "ctzti2_test.c", "divmodti4_test.c",
    "divti3_test.c", "ffsti2_test.c", "lshrti3_test.c", "modti3_test.c",
    "muloti4_test.c", "multi3_test.c", "mulvti3_test.c", "negti2_test.c",
    "negvti2_test.c", "parityti2_test.c", "popcountti2_test.c", "subvti3_test.c",
    "ucmpti2_test.c", "udivmodti4_test.c", "udivti3_test.c", "umodti3_test.c",
}

# --- ARM VFP hardware float (ARM-specific) ---
SKIP_TESTS |= {
    "adddf3vfp_test.c", "addsf3vfp_test.c", "divdf3vfp_test.c", "divsf3vfp_test.c",
    "eqdf2vfp_test.c", "eqsf2vfp_test.c", "extendsfdf2vfp_test.c",
    "fixdfsivfp_test.c", "fixsfsivfp_test.c", "fixunsdfsivfp_test.c",
    "fixunssfsivfp_test.c", "floatsidfvfp_test.c", "floatsisfvfp_test.c",
    "floatunssidfvfp_test.c", "floatunssisfvfp_test.c",
    "gedf2vfp_test.c", "gesf2vfp_test.c", "gtdf2vfp_test.c", "gtsf2vfp_test.c",
    "ledf2vfp_test.c", "lesf2vfp_test.c", "ltdf2vfp_test.c", "ltsf2vfp_test.c",
    "muldf3vfp_test.c", "mulsf3vfp_test.c", "nedf2vfp_test.c", "nesf2vfp_test.c",
    "negdf2vfp_test.c", "negsf2vfp_test.c", "subdf3vfp_test.c", "subsf3vfp_test.c",
    "truncdfsf2vfp_test.c", "unorddf2vfp_test.c", "unordsf2vfp_test.c",
}

# --- double-precision (df) - no 64-bit float support ---
SKIP_TESTS |= {
    "adddf3_test.c", "subdf3_test.c", "muldf3_test.c", "divdf3_test.c",
    "comparedf2_test.c", "powidf2_test.c",
    "fixdfdi_test.c", "fixdfsi_test.c", "fixdfti_test.c",
    "fixunsdfdi_test.c", "fixunsdfsi_test.c", "fixunsdfti_test.c",
    "floatdidf_test.c", "floatsidf_test.c", "floattidf_test.c",
    "floatundidf_test.c", "floatunsidf_test.c", "floatuntidf_test.c",
    "extendsfdf2_test.c", "truncdfsf2_test.c",
}

# --- long double / tf (128-bit float) ---
SKIP_TESTS |= {
    "addtf3_test.c", "subtf3_test.c", "multf3_test.c", "divtf3_test.c",
    "comparetf2_test.c",
    "eqtf2_test.c", "getf2_test.c", "gttf2_test.c",
    "letf2_test.c", "lttf2_test.c", "netf2_test.c", "unordtf2_test.c",
    "powitf2_test.c",
    "extenddftf2_test.c", "extendsftf2_test.c",
    "trunctfdf2_test.c", "trunctfsf2_test.c", "trunctfxf2_test.c",
    "fixtfdi_test.c", "fixtfsi_test.c", "fixtfti_test.c",
    "fixunstfdi_test.c", "fixunstfsi_test.c", "fixunstfti_test.c",
    "floatditf_test.c", "floatsitf_test.c", "floattitf_test.c",
    "floatunditf_test.c", "floatunsitf_test.c", "floatuntitf_test.c",
}

# --- extended precision (xf, 80-bit long double) ---
SKIP_TESTS |= {
    "powixf2_test.c",
    "extendxftf2_test.c",
    "fixxfdi_test.c", "fixxfti_test.c",
    "fixunsxfdi_test.c", "fixunsxfsi_test.c", "fixunsxfti_test.c",
    "floatdixf_test.c", "floattixf_test.c",
    "floatundixf_test.c", "floatuntixf_test.c",
}

# --- ti (128-bit) float conversions ---
SKIP_TESTS |= {
    "fixsfti_test.c", "fixunssfti_test.c",
    "floattisf_test.c", "floatuntisf_test.c",
}

# --- half-precision float (fp16) ---
SKIP_TESTS |= {
    "extendhfsf2_test.c", "extendhftf2_test.c",
    "truncdfhf2_test.c", "truncsfhf2_test.c", "trunctfhf2_test.c",
}

# --- Complex number operations ---
SKIP_TESTS |= {
    "divdc3_test.c", "divsc3_test.c", "divtc3_test.c", "divxc3_test.c",
    "muldc3_test.c", "mulsc3_test.c", "multc3_test.c", "mulxc3_test.c",
}

# --- Platform-specific / OS-dependent ---
SKIP_TESTS |= {
    "atomic_test.c", "clear_cache_test.c", "cpu_model_test.c",
    "enable_execute_stack_test.c", "gcc_personality_test.c",
    "trampoline_setup_test.c",
    "ctor_dtor.c",  # needs full CRT startup, not a builtin test
}

# --- Needs working libm comparison (fmaxf, logb, scalbn) ---
SKIP_TESTS |= {
    "compiler_rt_fmax_test.c", "compiler_rt_fmaxf_test.c", "compiler_rt_fmaxl_test.c",
    "compiler_rt_logb_test.c", "compiler_rt_logbf_test.c", "compiler_rt_logbl_test.c",
    "compiler_rt_scalbn_test.c", "compiler_rt_scalbnf_test.c", "compiler_rt_scalbnl_test.c",
}

# --- udivmoddi4 is 1.8MB source - too large for 64KB target ---
SKIP_TESTS |= {
    "udivmoddi4_test.c",
}

# --- float<->int64 conversions not in our softfloat library ---
SKIP_TESTS |= {
    "fixsfdi_test.c",       # float -> int64 (__fixsfdi)
    "fixunssfdi_test.c",    # float -> uint64 (__fixunssfdi)
    "floatdisf_test.c",     # int64 -> float (__floatdisf)
    "floatundisf_test.c",   # uint64 -> float (__floatundisf)
}

# --- __powisf2 not in our softfloat library ---
SKIP_TESTS |= {
    "powisf2_test.c",
}

# --- Builtins formerly not in libbuiltins.a (now added from compiler-rt) ---
# All 27 builtins below are now compiled from upstream compiler-rt and included
# in libbuiltins.a. Tests should pass.

# --- Tests that link-error due to __extendsfdf2 (double precision promotion in printf) ---
# The test source uses %a/%f format specifiers which promote float to double,
# or the test function signature uses double types internally.
SKIP_TESTS |= {
    "comparesf2_test.c",   # calls __extendsfdf2 (float->double for printf)
    "divsf3_test.c",       # calls __extendsfdf2
    "fixunssfsi_test.c",   # calls __extendsfdf2
}

# --- 64-bit shift ABI mismatch (FIXED) ---
# compiler-rt reference implementations now used in libbuiltins.a with correct
# 'int b' parameter type. Tests should pass.

# Note: mulsi3_test.c is NOT skipped. si_int = int32_t = long on TMS9900.
# INT_MAX (0x7FFF) is just a small si_int value. Large constant results
# (e.g. 34359730176) truncate to int32_t, matching __mulsi3's mod-2^32 behavior.

# Note: ARM aeabi tests are in the arm/ subdirectory and not picked up by our glob.


def build_support_objects(opt_level, build_dir, verbose=False):
    """Build crt_start.o, crt_stubs.o, softfloat objects, and fp32_builtins.o."""
    start_src = SHIM_DIR / "crt_start.S"
    stubs_src = SHIM_DIR / "crt_stubs.c"
    start_obj = build_dir / "crt_start.o"
    stubs_obj = build_dir / "crt_stubs.o"

    sf_cflags = ["--target=tms9900", "-O2", "-ffreestanding", "-fno-builtin"]

    # Assemble crt_start.S
    cmd = [
        str(CLANG), "--target=tms9900", "-c",
        str(start_src), "-o", str(start_obj),
    ]
    if verbose:
        print(f"  [CRT] {' '.join(cmd)}", flush=True)
    result = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
    if result.returncode != 0:
        print(f"Error building crt_start.o: {result.stderr}", file=sys.stderr)
        return None

    # Compile crt_stubs.c
    cmd = [
        str(CLANG)] + sf_cflags + [f"-I{INCLUDE_DIR}", "-c",
        str(stubs_src), "-o", str(stubs_obj),
    ]
    if verbose:
        print(f"  [CRT] {' '.join(cmd)}", flush=True)
    result = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
    if result.returncode != 0:
        print(f"Error building crt_stubs.o: {result.stderr}", file=sys.stderr)
        return None

    # Compile softfloat objects (always at -O2)
    sf_objs = []
    for sf_src in SOFTFLOAT_SRCS:
        src_path = SOFTFLOAT_DIR / sf_src
        obj_path = build_dir / f"sf_{sf_src.replace('.c', '.o')}"
        cmd = [str(CLANG)] + sf_cflags + ["-c", str(src_path), "-o", str(obj_path)]
        if verbose:
            print(f"  [SF]  {' '.join(cmd)}", flush=True)
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
        if result.returncode != 0:
            print(f"Error building {sf_src}: {result.stderr}", file=sys.stderr)
            return None
        sf_objs.append(obj_path)

    # Compile fp32_builtins.c (always at -O2)
    fp_obj = build_dir / "fp32_builtins.o"
    cmd = [str(CLANG)] + sf_cflags + [
        f"-I{INCLUDE_DIR}", "-c",
        str(FP_BUILTINS_SRC), "-o", str(fp_obj),
    ]
    if verbose:
        print(f"  [FP]  {' '.join(cmd)}", flush=True)
    result = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
    if result.returncode != 0:
        print(f"Error building fp32_builtins.o: {result.stderr}", file=sys.stderr)
        return None

    return {
        "start": start_obj,
        "stubs": stubs_obj,
        "softfloat": sf_objs,
        "fp_builtins": fp_obj,
    }


def compile_test(test_file, opt_level, build_dir, support, verbose=False):
    """Compile and link a single test. Returns (bin_path, error_string)."""
    name = test_file.stem
    obj = build_dir / f"{name}.o"
    elf = build_dir / f"{name}.elf"
    binf = build_dir / f"{name}.bin"

    # Compile
    cmd = [str(CLANG)] + CFLAGS_BASE + [f"-O{opt_level}"] + [
        "-c", str(test_file), "-o", str(obj)
    ]
    if verbose:
        print(f"    CC {' '.join(cmd)}", flush=True)
    try:
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=120)
    except subprocess.TimeoutExpired:
        return None, "compile_error: compile timeout (120s)"
    if result.returncode != 0:
        return None, f"compile_error: {result.stderr[:300]}"

    # Link: crt_start + test + stubs + softfloat + fp_builtins + libbuiltins
    link_objs = [
        str(support["start"]),
        str(obj),
        str(support["stubs"]),
    ] + [str(o) for o in support["softfloat"]] + [
        str(support["fp_builtins"]),
        str(BUILTINS),
    ]
    cmd = [str(LLD), "-T", str(BENCH_LD)] + link_objs + ["-o", str(elf)]
    if verbose:
        print(f"    LD {' '.join(cmd)}", flush=True)
    result = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
    if result.returncode != 0:
        return None, f"link_error: {result.stderr[:300]}"

    # Objcopy to binary
    cmd = [str(OBJCOPY), "-O", "binary", str(elf), str(binf)]
    result = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
    if result.returncode != 0:
        return None, f"objcopy_error: {result.stderr[:300]}"

    return binf, None


def run_test(bin_path, max_steps):
    """Run a test binary through tms9900-trace. Returns (passed, info_dict)."""
    cmd = [
        str(TMS9900_TRACE),
        "-l", "0x0000", "-e", "0x0000", "-w", "0xF000",
        "-n", str(max_steps),
        "-S",
        str(bin_path),
    ]
    try:
        output = subprocess.check_output(
            cmd, text=True, stderr=subprocess.STDOUT, timeout=120
        )
    except subprocess.TimeoutExpired:
        return False, {"halt": "timeout"}
    except subprocess.CalledProcessError as e:
        return False, {"halt": "error", "msg": str(e)}

    # Parse JSON output from emulator
    for line in reversed(output.strip().splitlines()):
        line = line.strip()
        if line.startswith("{"):
            try:
                info = json.loads(line)
                halt = info.get("halt", "unknown")
                regs = info.get("r", [])
                r0 = int(regs[0], 16) if regs else -1
                steps = info.get("steps", 0)
                passed = (halt == "idle" and r0 == 0)
                return passed, {"halt": halt, "r0": r0, "steps": steps}
            except (json.JSONDecodeError, IndexError, ValueError):
                continue
    return False, {"halt": "no_output"}


def main():
    parser = argparse.ArgumentParser(description="Run compiler-rt builtin tests on TMS9900")
    parser.add_argument("--opt", type=int, default=2,
                        help="Optimization level (default: 2)")
    parser.add_argument("--filter", default=None,
                        help="Only run tests matching this substring")
    parser.add_argument("--compile-only", action="store_true",
                        help="Only test compilation, don't execute")
    parser.add_argument("--max-steps", type=int, default=MAX_STEPS)
    parser.add_argument("--verbose", "-v", action="store_true")
    parser.add_argument("--stop-on-fail", action="store_true",
                        help="Stop after first failure")
    args = parser.parse_args()

    # Verify tools exist
    for tool, path in [("clang", CLANG), ("ld.lld", LLD),
                       ("llvm-objcopy", OBJCOPY), ("tms9900-trace", TMS9900_TRACE)]:
        if not pathlib.Path(path).exists():
            print(f"Error: {tool} not found at {path}", file=sys.stderr)
            return 2

    # Verify libbuiltins.a exists
    if not BUILTINS.exists():
        print(f"Error: libbuiltins.a not found at {BUILTINS}", file=sys.stderr)
        return 2

    # Collect test files (only top-level, not arm/ subdirectory)
    test_files = sorted(CRT_TEST_DIR.glob("*_test.c"))
    # Also include any non-test .c files that might be test-like
    other_tests = sorted(CRT_TEST_DIR.glob("*.c"))
    test_names = {f.name for f in test_files}
    for f in other_tests:
        if f.name not in test_names:
            test_files.append(f)
    test_files.sort()

    if args.filter:
        test_files = [f for f in test_files if args.filter in f.name]

    total_tests = len(test_files)
    skippable = sum(1 for f in test_files if f.name in SKIP_TESTS)
    print(f"Found {total_tests} test files ({skippable} to skip, "
          f"{total_tests - skippable} to run)", flush=True)

    with tempfile.TemporaryDirectory(prefix="crt_") as tmpdir:
        build_dir = pathlib.Path(tmpdir)

        # Build support objects
        print("Building support objects...", flush=True)
        support = build_support_objects(args.opt, build_dir, args.verbose)
        if support is None:
            return 2

        passed = 0
        failed = 0
        skipped = 0
        compile_errors = 0
        link_errors = 0
        results = {
            "pass": [],
            "fail": [],
            "skip": [],
            "compile_error": [],
            "link_error": [],
        }

        print(f"\n=== compiler-rt builtins at -O{args.opt} ===", flush=True)

        for i, test_file in enumerate(test_files):
            name = test_file.name

            # Check skip list
            if name in SKIP_TESTS:
                skipped += 1
                results["skip"].append(name)
                continue

            # Compile + link
            bin_path, error = compile_test(
                test_file, args.opt, build_dir,
                support, args.verbose
            )
            if error:
                if error.startswith("compile_error"):
                    compile_errors += 1
                    results["compile_error"].append((name, error))
                    print(f"  CERR {name}: {error[:120]}", flush=True)
                elif error.startswith("link_error"):
                    link_errors += 1
                    results["link_error"].append((name, error))
                    print(f"  LERR {name}: {error[:120]}", flush=True)
                else:
                    compile_errors += 1
                    results["compile_error"].append((name, error))
                    print(f"  ERR  {name}: {error[:120]}", flush=True)
                continue

            if args.compile_only:
                passed += 1
                results["pass"].append(name)
                if args.verbose:
                    print(f"  COMP {name}", flush=True)
                continue

            # Run on emulator
            ok, info = run_test(bin_path, args.max_steps)
            if ok:
                passed += 1
                results["pass"].append(name)
                steps = info.get("steps", 0)
                if args.verbose:
                    print(f"  PASS {name} ({steps} steps)", flush=True)
            else:
                failed += 1
                halt = info.get("halt", "?")
                r0 = info.get("r0", "?")
                results["fail"].append((name, halt, r0))
                print(f"  FAIL {name} (halt={halt}, R0=0x{r0:04X})" if isinstance(r0, int)
                      else f"  FAIL {name} (halt={halt}, R0={r0})", flush=True)
                if args.stop_on_fail:
                    break

            # Progress every 20 tests
            total_done = passed + failed + skipped + compile_errors + link_errors
            if total_done % 20 == 0 and not args.verbose:
                print(f"  ... {total_done}/{total_tests} done", flush=True)

        # Summary
        total = passed + failed + skipped + compile_errors + link_errors
        print(f"\n{'='*50}")
        print(f"Results at -O{args.opt}")
        print(f"{'='*50}")
        print(f"  Total:          {total}")
        print(f"  Passed:         {passed}")
        print(f"  Failed:         {failed}")
        print(f"  Skipped:        {skipped}")
        print(f"  Compile errors: {compile_errors}")
        print(f"  Link errors:    {link_errors}")

        if results["compile_error"]:
            print(f"\nCompile errors ({compile_errors}):")
            for name, err in results["compile_error"]:
                print(f"  {name}: {err[:200]}")

        if results["link_error"]:
            print(f"\nLink errors ({link_errors}):")
            for name, err in results["link_error"]:
                print(f"  {name}: {err[:200]}")

        if results["fail"]:
            print(f"\nRuntime failures ({failed}):")
            for name, halt, r0 in results["fail"]:
                if isinstance(r0, int):
                    print(f"  {name}: halt={halt}, R0=0x{r0:04X}")
                else:
                    print(f"  {name}: halt={halt}, R0={r0}")

        # Save results to JSON
        out_path = SHIM_DIR / f"results_O{args.opt}.json"
        # Convert tuples to lists for JSON serialization
        json_results = {}
        for key, val in results.items():
            if val and isinstance(val[0], tuple):
                json_results[key] = [list(v) for v in val]
            else:
                json_results[key] = val
        with open(out_path, "w") as f:
            json.dump(json_results, f, indent=2)
        print(f"\nResults saved to {out_path}")

    return 0 if (failed == 0 and compile_errors == 0 and link_errors == 0) else 1


if __name__ == "__main__":
    raise SystemExit(main())
