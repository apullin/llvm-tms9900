#!/usr/bin/env python3
"""Optional TMS9900 FreeRTOS execution and interrupt-barrier regressions."""

import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
FREERTOS = HERE.parent
ROOT = FREERTOS.parent
PORT = FREERTOS / "FreeRTOS-Kernel/portable/GCC/TMS9900"


def run(command):
    result = subprocess.run([str(arg) for arg in command], text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if result.returncode:
        raise RuntimeError(f"Command failed: {command}\n{result.stdout}{result.stderr}")
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--llvm-bin", type=Path, default=ROOT / "llvm-project/build/bin")
    parser.add_argument("--trace", type=Path, default=Path(os.environ.get(
        "TMS9900_TRACE", ROOT.parent / "tms9900-trace/build/tms9900-trace")))
    parser.add_argument("--opt", nargs="+", default=["0", "1", "2", "s", "z"],
                        choices=["0", "1", "2", "3", "s", "z"])
    parser.add_argument("--startup", type=Path, default=FREERTOS / "startup.S",
                        help="Override startup assembly for a before/after check")
    args = parser.parse_args()
    cc, ld = args.llvm_bin / "clang", args.llvm_bin / "ld.lld"
    objcopy = args.llvm_bin / "llvm-objcopy"
    include = ["-nostdlibinc", "-isystem", FREERTOS / "include", "-I", FREERTOS,
               "-I", FREERTOS / "FreeRTOS-Kernel/include", "-I", PORT]
    passed = total = 0

    def report(name, success, details=""):
        nonlocal passed, total
        total += 1
        passed += bool(success)
        print(f"{name}: {'PASS' if success else 'FAIL'} {details}", flush=True)

    with tempfile.TemporaryDirectory(prefix="tms9900-freertos-") as directory:
        out = Path(directory)
        start = out / "start.s"
        start.write_text(".text\n.globl _start\n_start:\n"
                         "LWPI 0x8300\nLI R10,0xf000\nBL @main\nIDLE\n")
        wrapper = out / "main.s"
        wrapper.write_text(".text\n.globl main\nmain:\nBL @check_startup\nIDLE\n")
        flat = out / "flat.ld"
        flat.write_text("ENTRY(_start)\nSECTIONS { . = 0x100; "
                        ".text : { *(.text*) } .rodata : { *(.rodata*) } "
                        ".data : { *(.data*) } .bss : { *(.bss*) } }\n")

        def assemble(source, target):
            run([cc, "--target=tms9900", "-c", source, "-o", target])

        def compile_c(source, target, opt, extra=()):
            run([cc, "--target=tms9900", "-O" + opt, "-ffreestanding", "-fno-builtin",
                 "-ffunction-sections", "-fdata-sections", *include, *extra,
                 "-c", source, "-o", target])

        def execute(name, objects, script, load, poison=None):
            elf, binary = out / "test.elf", out / "test.bin"
            run([ld, "--gc-sections", "-T", script, *objects, "-o", elf])
            run([objcopy, "-O", "binary", elf, binary])
            if poison is not None:
                begin, size = poison
                image = bytearray(binary.read_bytes())
                end = begin - load + size
                image.extend(bytes(max(0, end + 1 - len(image))))
                image[begin - load:end] = bytes([0xcc]) * size
                image[begin - load - 1] = 0x5a
                image[end] = 0x3c
                binary.write_bytes(image)
            state = json.loads(run([args.trace, "-S", "--no-loop-detect", "-n", "1000000",
                                    "-l", hex(load), "-e", "0x100", "-w", "0x8300",
                                    binary]).stdout.splitlines()[-1])
            report(name, state["halt"] == "idle" and int(state["r"][0], 16) == 0,
                   f"R0={state['r'][0]}, halt={state['halt']}")

        assemble(start, out / "start.o")
        assemble(wrapper, out / "wrapper.o")
        assemble(args.startup, out / "startup.o")
        for opt in args.opt:
            compile_c(PORT / "port.c", out / "port.o", opt)
            compile_c(HERE / "stack.c", out / "stack.o", opt)
            execute(f"task stack -O{opt}", [out / "start.o", out / "stack.o", out / "port.o"],
                    flat, 0x100)
            ir = run([cc, "--target=tms9900", "-O" + opt, "-ffreestanding", *include,
                      "-S", "-emit-llvm", HERE / "barriers.c", "-o", "-"]).stdout
            barriers = [line for line in ir.splitlines() if 'asm sideeffect "LIMI ' in line]
            report(f"interrupt barriers -O{opt}", len(barriers) == 2 and all(
                "~{memory}" in line and "~{cc}" in line for line in barriers))
            compile_c(HERE / "barriers.c", out / "barriers.o", opt)
            for address, size in [(0x7001, 0), (0x7000, 1), (0x7000, 3),
                                  (0x7001, 1), (0x7001, 3),
                                  (0x7ffe, 3), (0x9001, 3)]:
                compile_c(HERE / "startup.c", out / "test.o", opt,
                          [f"-DSECTION_BYTES={size}"])
                script = out / "startup.ld"
                script.write_text("ENTRY(_start)\nSECTIONS { .vectors 0 : { KEEP(*(.vectors)) } "
                                  ".startup 0x100 : { *(.startup) } .text : { *(.text*) } "
                                  ".rodata : { *(.rodata*) } .data : { *(.data*) } "
                                  f".bss {address} : {{ __bss_start = .; *(.bss*) "
                                  "__bss_end = .; } }\n")
                execute(f"startup -O{opt} BSS={address:04x}+{size}",
                        [out / "startup.o", out / "wrapper.o", out / "test.o"],
                        script, 0, (address, size))

        # An oversized program must fail before it can overwrite demo markers
        # or either reserved workspace/stack.
        overflow = out / "overflow.s"
        overflow.write_text(".text\n.globl main\nmain:\nB *R11\n"
                            ".bss\n.space 32768\n")
        assemble(overflow, out / "overflow.o")
        result = subprocess.run([str(ld), "-T", str(FREERTOS / "linker.ld"),
                                 str(out / "startup.o"), str(out / "overflow.o"),
                                 "-o", str(out / "overflow.elf")], text=True,
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        report("linker rejects reserved-memory overlap", result.returncode != 0 and
               "region 'IMAGE'" in result.stderr and "overflowed" in result.stderr,
               result.stderr.strip() if result.returncode == 0 else "")

        # Real scheduler paths: timer preemption, queue waits and software yields.
        for opt in args.opt:
            build = out / ("demo-O" + opt)
            run(["make", "-C", FREERTOS, "-j4", f"BUILDDIR={build}",
                 f"LLVM_BIN={args.llvm_bin}",
                 "CFLAGS=" + " ".join(str(arg) for arg in ["--target=tms9900", "-O" + opt,
                                                          "-ffreestanding", "-fno-builtin", *include])])
            for name, steps in [("freertos_test", 500000), ("freertos_queue", 1000000)]:
                result = run([args.trace, "-S", "--timer=1:50000", "--no-loop-detect",
                              "-l", "0x0", "-e", "0x100", "-w", "0x8300", "-n", steps,
                              "-d", "0x7f00:8", build / (name + ".bin")])
                dump = re.search(r"7F00: ((?:[0-9A-F]{2} ){7}[0-9A-F]{2})", result.stderr)
                words = [] if not dump else [int.from_bytes(bytes.fromhex(dump[1])[i:i+2], "big")
                                             for i in range(0, 8, 2)]
                if name == "freertos_test":
                    success = len(words) == 4 and words[0] > 0 and words[1] > 0
                else:
                    # We can stop during a round; each worker may have completed
                    # either ROUNDS or ROUNDS+1 jobs at that instant.
                    success = len(words) == 4 and words[0] > 0 and all(
                        words[0] <= count <= words[0] + 1 for count in words[1:])
                report(f"{name} -O{opt}", success, str(words))

    print(f"{passed}/{total} FreeRTOS tests passed")
    return int(passed != total)


if __name__ == "__main__":
    raise SystemExit(main())
