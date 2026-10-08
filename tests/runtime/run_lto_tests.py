#!/usr/bin/env python3
"""Execute TU-aware full-LTO calls and local/global name collisions."""

import argparse
import json
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def run(command, cwd=None):
    result = subprocess.run([str(arg) for arg in command], cwd=cwd, text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if result.returncode:
        raise RuntimeError(f"{command}\n{result.stdout}{result.stderr}")
    return result.stdout


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--llvm-bin", type=Path, default=ROOT / "llvm-project/build/bin")
    parser.add_argument("--trace", type=Path, default=ROOT.parent / "tms9900-trace/build/tms9900-trace")
    parser.add_argument("--opt", nargs="+", choices=["0", "1", "2", "3", "s", "z"],
                        default=["0", "1", "2", "3", "s", "z"])
    args = parser.parse_args()
    tools, trace = args.llvm_bin.resolve(), args.trace.resolve()
    count = 0
    failures = []
    with tempfile.TemporaryDirectory(prefix="tms9900-lto-runtime-") as directory:
        out = Path(directory)
        (out / "a.c").write_text('''
typedef unsigned short u16;
_Static_assert(sizeof(u16) == 2, "16-bit unsigned short required");
static volatile u16 shared __attribute__((section(".payload"))) = 0x1122;
static volatile u16 collision __attribute__((section(".payload"))) = 0x0203;
extern u16 transform(u16);
__attribute__((section(".text.caller"))) u16 dispatch(u16 x) {
    return transform(x) ^ shared ^ collision;
}
''')
        (out / "b.c").write_text('''
typedef unsigned short u16;
static volatile u16 shared __attribute__((section(".payload"))) = 0x3344;
volatile u16 collision __attribute__((section(".payload"))) = 0x0506;
__attribute__((always_inline, section(".text.callee"))) u16 transform(u16 x) {
    return x + shared + collision;
}
''')
        (out / "start.s").write_text('''
.section .text.start,"ax",@progbits
.globl _start
_start:
  LWPI 0xf000
  LI R10,0xfffc
  LI R13,0xd013
  LI R14,0xd014
  LI R15,0xd015
  MOV @test_input,R0
  BL @dispatch
  IDLE
.section .input,"aw",@progbits
.globl test_input
test_input:
  .short 0
''')
        (out / "script.ld").write_text('''
ENTRY(_start)
SECTIONS {
  .start 0x6000 : { KEEP(*(.text.start)) }
  .caller 0x6100 : { a.o(.text.caller) }
  .callee 0x6200 : { b.o(.text.callee) }
  .a 0xa000 : { KEEP(a.o(.payload)) }
  .b 0xa100 : { KEEP(b.o(.payload)) }
  .input 0xa200 : { *(.input) }
}
''')
        run([tools / "clang", "--target=tms9900", "-c", "start.s", "-o", "start.o"], out)
        for opt in args.opt:
            for name in ("a", "b"):
                run([tools / "clang", "--target=tms9900", "-O" + opt,
                     "-ffreestanding", "-fno-builtin", "-flto=full",
                     "-flto-linker-scripts", "-c", name + ".c", "-o", name + ".o"], out)
            for order in (("a.o", "b.o"), ("b.o", "a.o")):
                run([tools / "ld.lld", "--lto-linker-scripts", "--lto-O" + opt,
                     "--gc-sections", "-T", "script.ld", "start.o", *order,
                     "-o", "test.elf"], out)
                symbols = {fields[2]: int(fields[0], 16)
                           for line in run([tools / "llvm-nm", "test.elf"], out).splitlines()
                           if len(fields := line.split()) == 3 and fields[1].lower() != "u"}
                # Check actual native placement as well as returned values.
                for name, address in (("_start", 0x6000), ("dispatch", 0x6100),
                                      ("transform", 0x6200), ("test_input", 0xa200)):
                    if symbols.get(name) != address:
                        raise RuntimeError(f"-O{opt} {order}: {name} placed at {symbols.get(name)}")
                run([tools / "llvm-objcopy", "-O", "binary", "test.elf", "test.bin"], out)
                image = bytearray((out / "test.bin").read_bytes())
                offset = symbols["test_input"] - 0x6000
                group_failures = []
                for seed in (0, 1, 0x7fff, 0x8000, 0xffff):
                    # Only the native assembly input slot changes between runs;
                    # no C checker or target arithmetic computes the oracle.
                    image[offset:offset + 2] = seed.to_bytes(2, "big")
                    (out / "test.bin").write_bytes(image)
                    state = json.loads(run([trace, "-S", "--no-loop-detect", "-l", "0x6000",
                                            "-e", "0x6000", "-n", "10000", out / "test.bin"]))
                    expected = ((seed + 0x3344 + 0x0506) & 0xffff) ^ 0x1122 ^ 0x0203
                    registers = [int(value, 16) for value in state["r"]]
                    passed = (state["halt"] == "idle" and registers[0] == expected
                              and registers[10] == 0xfffc
                              and registers[13:16] == [0xd013, 0xd014, 0xd015])
                    count += 1
                    if not passed:
                        group_failures.append(f"seed={seed:04x}, halt={state['halt']}, "
                                              f"R0={registers[0]:04x}, expected={expected:04x}")
                label = f"-O{opt} {'forward' if order[0] == 'a.o' else 'reverse'}"
                print(f"{label}: {'FAIL' if group_failures else 'PASS'} (5 inputs)", flush=True)
                for failure in group_failures:
                    failures.append(label + ": " + failure)
                    print(failures[-1], flush=True)
    print(f"{count - len(failures)}/{count} full-LTO execution checks passed")
    return bool(failures)


if __name__ == "__main__":
    raise SystemExit(main())
