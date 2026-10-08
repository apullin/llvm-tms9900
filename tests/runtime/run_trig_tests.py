#!/usr/bin/env python3
"""Check binary32 sine/cosine against independent host-libm reference values.

Only the test data is generated on the host. The functions and their soft-float
builtins execute in tms9900-trace. No simulator dependency is added to LLVM.
"""

import argparse
import json
import math
import os
from pathlib import Path
import random
import re
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def run(command):
    result = subprocess.run([str(arg) for arg in command], text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if result.returncode:
        raise RuntimeError(f"Command failed: {command}\n{result.stdout}")
    return result.stdout


def bits(value):
    return struct.unpack(">I", struct.pack(">f", value))[0]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--llvm-bin", type=Path, default=ROOT / "llvm-project/build/bin")
    parser.add_argument("--trace", type=Path, default=Path(os.environ.get(
        "TMS9900_TRACE", ROOT.parent / "tms9900-trace/build/tms9900-trace")))
    parser.add_argument("--opt", nargs="+", default=["0", "1", "2", "s", "z"],
                        choices=["0", "1", "2", "3", "s", "z"])
    parser.add_argument("--sincos-source", type=Path,
                        help="Override sinf/cosf source for before/after comparisons")
    args = parser.parse_args()
    args.llvm_bin = args.llvm_bin.resolve()
    cc, ld = args.llvm_bin / "clang", args.llvm_bin / "ld.lld"
    ar, objcopy = args.llvm_bin / "llvm-ar", args.llvm_bin / "llvm-objcopy"

    # Signed zero, subnormals, quadrant boundaries, the former 16-bit quadrant
    # overflow region, huge finite values and both quiet/signaling NaNs.
    magnitudes = [0, 1, 0x7fffff, 0x800000, 0x3f000000, 0x3f490fd9,
                  0x3f490fda, 0x3f490fdb, 0x3f490fdc, 0x3fc90fda,
                  0x3fc90fdb, 0x3fc90fdc, 0x40490fda, 0x40490fdb,
                  0x40490fdc, bits(100.0), bits(10000.0), bits(51470.0),
                  bits(60000.0), bits(1e6), bits(1e10), bits(1e20),
                  0x7f7fffff, 0x7f800000, 0x7fc01234, 0x7f800001]
    inputs = [word | sign for word in magnitudes for sign in [0, 0x80000000]]
    rng = random.Random(9900)
    inputs += [rng.getrandbits(32) for _ in range(128)]
    inputs += [bits(rng.uniform(-10000, 10000)) for _ in range(64)]
    records = []
    expected = []
    for word in inputs:
        value = struct.unpack(">f", struct.pack(">I", word))[0]
        sine, cosine = (bits(math.sin(value)), bits(math.cos(value))) if \
            math.isfinite(value) else (0x7fc00000, 0x7fc00000)
        records.append(f"{{0x{word:08x}UL,0x{sine:08x}UL,0x{cosine:08x}UL}}")
        expected += [sine, cosine]
    source = r'''
#include <stdint.h>
#include <math.h>
extern float sinf(float);
extern float cosf(float);
struct test { uint32_t input, sine, cosine; };
static const struct test tests[] = { RECORDS };
volatile uint32_t results[2 * sizeof(tests) / sizeof(tests[0])];
volatile uint32_t last_input, last_actual, last_expected;
static uint32_t ordered(uint32_t word) {
    return word & 0x80000000UL ? ~word : word | 0x80000000UL;
}
static int correct(uint32_t actual, uint32_t expected) {
    if ((expected & 0x7fffffffUL) == 0)
        return actual == expected; /* preserve the sign of zero */
    if ((expected & 0x7f800000UL) == 0x7f800000UL)
        return (actual & 0x7f800000UL) == 0x7f800000UL &&
               (actual & 0x007fffffUL) != 0;
    if ((actual & 0x7f800000UL) == 0x7f800000UL)
        return 0;
    uint32_t a = ordered(actual), b = ordered(expected);
    return (a > b ? a - b : b - a) <= 2; /* two binary32 ULPs */
}
int main(void) {
    static const uint32_t rounding[][2] = {
        {0x00000000UL,0x00000000UL}, {0x80000000UL,0x80000000UL},
        {0x3effffffUL,0x00000000UL}, {0xbeffffffUL,0x80000000UL},
        {0x3f000000UL,0x3f800000UL}, {0xbf000000UL,0xbf800000UL},
        {0x3fc00000UL,0x40000000UL}, {0xbfc00000UL,0xc0000000UL}
    };
    for (unsigned i = 0; i < sizeof(rounding) / sizeof(rounding[0]); ++i) {
        union { uint32_t word; float value; } x = {rounding[i][0]}, result;
        result.value = roundf(x.value);
        if (result.word != rounding[i][1])
            return 2 * sizeof(tests) / sizeof(tests[0]) + 3;
    }
    for (unsigned i = 0; i < sizeof(tests) / sizeof(tests[0]); ++i) {
        union { uint32_t word; float value; } x = {tests[i].input}, result;
        last_input = x.word;
        uint32_t magnitude = x.word & 0x7fffffffUL;
        if (!!isnan(x.value) != (magnitude > 0x7f800000UL) ||
            !!isinf(x.value) != (magnitude == 0x7f800000UL) ||
            !!isfinite(x.value) != (magnitude < 0x7f800000UL) ||
            !!issignaling(x.value) != (magnitude > 0x7f800000UL &&
                                      (magnitude & 0x00400000UL) == 0) ||
            !!signbit(x.value) != !!(x.word & 0x80000000UL))
            return 2 * sizeof(tests) / sizeof(tests[0]) + 1;
        int category = magnitude > 0x7f800000UL ? FP_NAN :
                       magnitude == 0x7f800000UL ? FP_INFINITE :
                       magnitude == 0 ? FP_ZERO :
                       magnitude < 0x00800000UL ? FP_SUBNORMAL : FP_NORMAL;
        if (fpclassify(x.value) != category ||
            !!isnormal(x.value) != (category == FP_NORMAL))
            return 2 * sizeof(tests) / sizeof(tests[0]) + 2;
        result.value = sinf(x.value);
        results[2 * i] = result.word;
        last_actual = result.word;
        last_expected = tests[i].sine;
        if (!correct(result.word, tests[i].sine))
            return 2 * i + 1;
        result.value = cosf(x.value);
        results[2 * i + 1] = result.word;
        last_actual = result.word;
        last_expected = tests[i].cosine;
        if (!correct(result.word, tests[i].cosine))
            return 2 * i + 2;
    }
    return 0;
}
'''.replace("RECORDS", ",\n".join(records))
    passed = 0
    with tempfile.TemporaryDirectory(prefix="tms9900-trig-") as directory:
        out = Path(directory)
        (out / "test.c").write_text(source)
        (out / "start.s").write_text(".text\n.globl _start\n_start:\n"
                                     "LWPI 0xf000\nLI R10,0xfffc\nBL @main\nIDLE\n")
        (out / "link.ld").write_text("ENTRY(_start)\nSECTIONS { . = 0x100; "
                                     ".text : { *(.text*) } .rodata : { *(.rodata*) } "
                                     ".data : { *(.data*) } .bss : { *(.bss*) } }\n")
        run([cc, "--target=tms9900", "-c", out / "start.s", "-o", out / "start.o"])
        for opt in args.opt:
            flags = f"--target=tms9900 -O{opt} -ffreestanding -fno-builtin"
            builtin_dir, libm_dir = out / ("builtins-" + opt), out / ("libm-" + opt)
            # Simulate upgrading an old build directory. Updating an archive
            # with ar rcs must not retain a removed override member ahead of
            # the real picolibc definitions.
            libm_dir.mkdir()
            stale = out / "stale.s"
            stale.write_text(".text\n.globl sinf\nsinf:\nCLR R0\nCLR R1\nB *R11\n")
            run([cc, "--target=tms9900", "-c", stale,
                 "-o", libm_dir / "sincosf_tiny.o"])
            run([ar, "rcs", libm_dir / "libm.a", libm_dir / "sincosf_tiny.o"])
            # This represents an earlier build, not an archive manually altered
            # after today's Makefile. macOS's system make has coarse mtimes.
            os.utime(libm_dir / "libm.a", (1, 1))
            run(["make", "-C", ROOT / "libtms9900/builtins", "-j4",
                 f"CC={cc}", f"AR={ar}", f"BUILD={builtin_dir}", f"CFLAGS={flags} -c"])
            run(["make", "-C", ROOT / "libtms9900/libm", "-j4",
                 f"CC={cc}", f"AR={ar}", f"BUILD={libm_dir}",
                 "CFLAGS=" + flags + " -I ../picolibc/libc/include "
                 "-I ../picolibc/libm/common -I include -include include/picolibc.h "
                 "-D_COMPILING_NEWLIB", libm_dir / "libm.a"])
            run([cc, *flags.split(), "-I", ROOT / "libtms9900/picolibc/libc/include",
                 "-I", ROOT / "libtms9900/libm/include",
                 "-include", ROOT / "libtms9900/libm/include/picolibc.h",
                 "-c", out / "test.c", "-o", out / "test.o"])
            objects = [out / "start.o", out / "test.o"]
            if args.sincos_source:
                run([cc, *flags.split(), "-c", args.sincos_source, "-o", out / "override.o"])
                objects.append(out / "override.o")
            elf, binary = out / "test.elf", out / "test.bin"
            run([ld, "-T", out / "link.ld", *objects, libm_dir / "libm.a",
                 builtin_dir / "libbuiltins.a", "-o", elf])
            run([objcopy, "-O", "binary", elf, binary])
            if len(binary.read_bytes()) > 0xe000:
                raise RuntimeError("Test image overlaps reserved workspace/stack")
            symbols = {fields[2]: fields[0] for line in run(
                [args.llvm_bin / "llvm-nm", elf]).splitlines()
                       if len(fields := line.split()) == 3}
            dump = [item for name in ["last_input", "last_actual", "last_expected"]
                    for item in ["-d", "0x" + symbols[name] + ":4"]]
            dump += ["-d", "0x" + symbols["results"] + ":" + str(len(expected) * 4)]
            result = run([args.trace, "-S", "--no-loop-detect", "-n", "100000000",
                          "-l", "0x100", "-e", "0x100", *dump, binary])
            state = json.loads(result.splitlines()[-1])
            code = int(state["r"][0], 16)
            success = state["halt"] == "idle" and code == 0
            # Verify the stored bits independently on the host as well: a
            # compiler bug in the C test's comparisons must not hide a failure.
            if success:
                memory = {}
                for line in result.splitlines():
                    match = re.match(r"\s+([0-9A-F]{4}): (.*?)\s+\|", line)
                    if match:
                        address = int(match[1], 16)
                        memory.update({address + i: value for i, value in
                                       enumerate(bytes.fromhex(match[2]))})
                begin = int(symbols["results"], 16)
                actual = [int.from_bytes(bytes(memory[begin + i + j] for j in range(4)), "big")
                          for i in range(0, len(expected) * 4, 4)]
                maximum_ulp = 0
                for got, want in zip(actual, expected):
                    if want & 0x7fffffff == 0:
                        success &= got == want
                    elif want & 0x7f800000 == 0x7f800000:
                        success &= (got & 0x7fc00000) == 0x7fc00000
                    elif got & 0x7f800000 == 0x7f800000:
                        success = False
                    else:
                        order = lambda word: (~word & 0xffffffff) if word & 0x80000000 else \
                            word | 0x80000000
                        distance = abs(order(got) - order(want))
                        maximum_ulp = max(maximum_ulp, distance)
                        success &= distance <= 2
                print(f"  independent RAM check: max finite error={maximum_ulp} ULP", flush=True)
            passed += success
            print(f"sinf/cosf -O{opt}: {'PASS' if success else 'FAIL'} "
                  f"({len(inputs)} inputs, R0={code}, halt={state['halt']}, "
                  f"steps={state['steps']})", flush=True)
            if not success:
                if code and code <= 2 * len(inputs):
                    print(f"  input=0x{inputs[(code - 1)//2]:08x}, "
                          f"function={'sinf' if code % 2 else 'cosf'}")
                print(result)
    print(f"{passed}/{len(args.opt)} sine/cosine matrices passed")
    return int(passed != len(args.opt))


if __name__ == "__main__":
    raise SystemExit(main())
