#!/usr/bin/env python3
"""
TMS9900 Benchmark Suite Runner

Compiles all benchmarks at multiple optimization levels and runs them
through tms9900-trace, collecting code size and cycle count metrics.

Usage:
    python3 run_benchmarks.py                   # Run all benchmarks at -O0, -O1, -O2
    python3 run_benchmarks.py --opt 2           # Run all at -O2 only
    python3 run_benchmarks.py --bench fib crc32 # Run specific benchmarks
    python3 run_benchmarks.py --csv             # Output as CSV
"""
import argparse
import json
import os
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
LLVM_BIN = ROOT / "llvm-project" / "build" / "bin"
CLANG = LLVM_BIN / "clang"
LLD = LLVM_BIN / "ld.lld"
OBJCOPY = LLVM_BIN / "llvm-objcopy"
SIZE = LLVM_BIN / "llvm-size"
HOME = pathlib.Path.home()
TMS9900_TRACE = os.environ.get(
    "TMS9900_TRACE",
    str(HOME / "personal" / "ti99" / "tms9900-trace" / "build" / "tms9900-trace"),
)
BENCH_DIR = pathlib.Path(__file__).resolve().parent

# All benchmarks in the suite
ALL_BENCHMARKS = [
    "fib",
    "bubble_sort",
    "deep_recursion",
    "crc32",
    "q7_8_matmul",
    "json_parse",
    "string_torture",
    "float_torture",
    "bitops_torture",
    "vertex3d",
    "huffman",
    "long_torture",
    "heap4",
    "i64_torture",
    "cpp_test",
    "lambda_test",
    "mi_test",
    "cpp_adv_test",
    "stl_test",
    "stl_util_test",
]

# Benchmarks that halt via IDLE on success
IDLE_HALT = {
    "bubble_sort", "deep_recursion", "crc32", "q7_8_matmul",
    "json_parse", "string_torture", "float_torture", "bitops_torture",
    "vertex3d",
    "huffman",
    "long_torture",
    "heap4",
    "i64_torture",
    "cpp_test",
    "lambda_test",
    "mi_test",
    "cpp_adv_test",
    "stl_test",
    "stl_util_test",
}

LOAD_ADDR = 0x0000
MAX_STEPS = 5_000_000  # 5M instructions should be enough for any benchmark


def run_cmd(cmd, **kwargs):
    """Run a command, raising on failure."""
    return subprocess.run(cmd, check=True, **kwargs)


def builtins_suffix(builtins_mode: str) -> str:
    if builtins_mode == "lto-archive":
        return "-blto-gc"
    if builtins_mode == "local":
        return "-local"
    return ""


def build_dir_name(opt: str, lto: bool, builtins_mode: str) -> str:
    name = f"O{opt}"
    if lto:
        name += "-lto"
    name += builtins_suffix(builtins_mode)
    return name


def opt_label(opt: str, lto: bool, builtins_mode: str) -> str:
    label = f"O{opt}"
    if lto:
        label += "+LTO"
    if builtins_mode == "lto-archive":
        label += "+BLTO+GC"
    elif builtins_mode == "local":
        label += "+LOCAL"
    return label


def build_benchmark(bench: str, opt: str, lto: bool, builtins_mode: str) -> pathlib.Path:
    """Build a benchmark using make, return path to .bin file."""
    make_cmd = [
        "make",
        "-C",
        str(BENCH_DIR),
        bench,
        f"OPT={opt}",
        f"BUILTINS_MODE={builtins_mode}",
    ]
    if lto:
        make_cmd.append("LTO=1")
    run_cmd(
        make_cmd,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.PIPE,
    )
    return BENCH_DIR / "build" / build_dir_name(opt, lto, builtins_mode) / f"{bench}.bin"


def get_code_size(bench: str, opt: str, lto: bool, builtins_mode: str) -> int:
    """Get .text section size from ELF."""
    elf = BENCH_DIR / "build" / build_dir_name(opt, lto, builtins_mode) / f"{bench}.elf"
    out = subprocess.check_output(
        [str(SIZE), "-A", str(elf)], text=True
    )
    for line in out.splitlines():
        parts = line.split()
        if len(parts) >= 2 and parts[0] == ".text":
            return int(parts[1])
    return 0


def run_trace(bin_path: pathlib.Path, max_steps: int) -> dict:
    """Run benchmark through tms9900-trace and return metrics."""
    cmd = [
        str(TMS9900_TRACE),
        "-l", f"0x{LOAD_ADDR:04X}",
        "-e", f"0x{LOAD_ADDR:04X}",
        "-w", "0xF000",
        "-n", str(max_steps),
        "-S",  # JSON summary output
        str(bin_path),
    ]
    try:
        output = subprocess.check_output(
            cmd, text=True, stderr=subprocess.STDOUT, timeout=60
        )
    except subprocess.TimeoutExpired:
        return {"halt": "timeout", "steps": max_steps, "clk": 0}
    except subprocess.CalledProcessError as e:
        return {"halt": "error", "steps": 0, "clk": 0, "error": str(e)}

    # Parse JSON from last line of output
    for line in reversed(output.strip().splitlines()):
        line = line.strip()
        if line.startswith("{"):
            try:
                return json.loads(line)
            except json.JSONDecodeError:
                continue
    return {"halt": "unknown", "steps": 0, "clk": 0}


def run_one(bench: str, opt: str, lto: bool, builtins_mode: str) -> dict:
    """Build and run a single benchmark, return results dict."""
    try:
        bin_path = build_benchmark(bench, opt, lto, builtins_mode)
    except subprocess.CalledProcessError as e:
        return {
            "bench": bench, "opt": opt_label(opt, lto, builtins_mode),
            "status": "build_fail", "error": e.stderr.decode() if e.stderr else str(e),
        }

    code_size = get_code_size(bench, opt, lto, builtins_mode)
    info = run_trace(bin_path, MAX_STEPS)

    halt = info.get("halt", "unknown")
    steps = info.get("steps", 0)
    clk = info.get("clk", 0)

    # Determine pass/fail
    if halt == "idle":
        status = "PASS"
    elif halt == "stop" or halt == "step_limit":
        status = "LIMIT"
    else:
        status = halt.upper()

    return {
        "bench": bench, "opt": opt_label(opt, lto, builtins_mode),
        "status": status,
        "code_size": code_size,
        "steps": steps,
        "cycles": clk,
    }


def print_table(results: list) -> None:
    """Print results as a formatted table."""
    header = f"{'Benchmark':<20} {'Opt':<5} {'Status':<8} {'Code':<8} {'Steps':<12} {'Cycles':<12}"
    print(header)
    print("-" * len(header))
    for r in results:
        print(
            f"{r.get('bench', '?'):<20} {r.get('opt', '?'):<5} {r.get('status', '?'):<8} "
            f"{r.get('code_size', 0):<8} {r.get('steps', 0):<12} {r.get('cycles', 0):<12}"
        )


def print_csv(results: list) -> None:
    """Print results as CSV."""
    print("benchmark,opt,status,code_bytes,steps,cycles")
    for r in results:
        print(
            f"{r.get('bench', '?')},{r.get('opt', '?')},{r.get('status', '?')},"
            f"{r.get('code_size', 0)},{r.get('steps', 0)},{r.get('cycles', 0)}"
        )


def main() -> int:
    parser = argparse.ArgumentParser(description="Run TMS9900 benchmark suite")
    parser.add_argument("--opt", action="append", default=[],
                        help="Optimization levels (0, 1, 2, s). Can repeat.")
    parser.add_argument("--bench", action="append", default=[],
                        help="Specific benchmarks to run. Can repeat.")
    parser.add_argument("--csv", action="store_true",
                        help="Output as CSV")
    parser.add_argument("--lto", action="store_true",
                        help="Enable full LTO for the benchmark build")
    parser.add_argument("--builtins-mode", choices=["prebuilt", "lto-archive", "local"],
                        default="prebuilt",
                        help="Builtin library mode for links (default: prebuilt)")
    parser.add_argument("--max-steps", type=int, default=MAX_STEPS,
                        help="Max instructions per run")
    args = parser.parse_args()

    # Validate tools exist
    if not CLANG.exists():
        print(f"Error: clang not found at {CLANG}", file=sys.stderr)
        return 2
    if not pathlib.Path(TMS9900_TRACE).exists():
        print(f"Error: tms9900-trace not found at {TMS9900_TRACE}", file=sys.stderr)
        print("Set TMS9900_TRACE env var to override.", file=sys.stderr)
        return 2

    opts = args.opt or ["0", "1", "2"]
    benchmarks = args.bench or ALL_BENCHMARKS

    # Validate benchmark names
    for b in benchmarks:
        if b not in ALL_BENCHMARKS:
            print(f"Unknown benchmark: {b}", file=sys.stderr)
            print(f"Available: {', '.join(ALL_BENCHMARKS)}", file=sys.stderr)
            return 2

    results = []
    for opt in opts:
        for bench in benchmarks:
            r = run_one(bench, opt, args.lto, args.builtins_mode)
            results.append(r)
            if not args.csv:
                status_char = "+" if r["status"] == "PASS" else "-"
                print(
                    f"  [{status_char}] {bench} {opt_label(opt, args.lto, args.builtins_mode)}: "
                    f"{r['status']} "
                    f"({r.get('code_size', 0)}B, {r.get('steps', 0)} steps, "
                    f"{r.get('cycles', 0)} cycles)"
                )

    print()
    if args.csv:
        print_csv(results)
    else:
        print_table(results)

    # Summary
    passed = sum(1 for r in results if r["status"] == "PASS")
    total = len(results)
    print(f"\n{passed}/{total} passed")

    return 0 if passed == total else 1


if __name__ == "__main__":
    raise SystemExit(main())
