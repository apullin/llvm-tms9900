#!/usr/bin/env python3
"""Run freestanding runtime/startup regressions in the optional CPU simulator.

These tests belong to the tools repository; LLVM's in-tree regressions have
no dependency on tms9900-trace. Build the toolchain before running this script.
"""

import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent


def run(command):
    command = [str(arg) for arg in command]
    result = subprocess.run(command, text=True, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT)
    if result.returncode:
        raise RuntimeError(f"Command failed ({result.returncode}): {command}\n"
                           f"{result.stdout}")
    return result.stdout


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--llvm-bin", type=Path,
                        default=ROOT / "llvm-project/build/bin")
    parser.add_argument("--trace", type=Path, default=Path(os.environ.get(
        "TMS9900_TRACE", ROOT.parent / "tms9900-trace/build/tms9900-trace")))
    parser.add_argument("--llc", type=Path,
                        help="Override llc for comparison with an older backend")
    parser.add_argument("--crt", type=Path, default=ROOT / "cart_example/crt0.s",
                        help="Override startup assembly for before/after checks")
    parser.add_argument("--opt", nargs="+", choices=["0", "1", "2", "3", "s", "z"],
                        default=["0", "1", "2", "s", "z"])
    parser.add_argument("--filter", choices=["peephole", "memset", "malloc", "crt"])
    args = parser.parse_args()
    cc = args.llvm_bin / "clang"
    ld = args.llvm_bin / "ld.lld"
    objcopy = args.llvm_bin / "llvm-objcopy"
    failures = []
    count = 0

    with tempfile.TemporaryDirectory(prefix="tms9900-runtime-") as directory:
        out = Path(directory)
        start = out / "start.s"
        start.write_text(".text\n.globl _start\n_start:\n"
                         "LWPI 0xf000\nLI R10,0xfffc\nBL @main\nIDLE\n")
        main_wrapper = out / "main.s"
        main_wrapper.write_text(".text\n.globl main\nmain:\n"
                                "BL @check_startup\nIDLE\n"
                                ".section .cart_header,\"a\"\n.space 36\n")
        flat_script = out / "flat.ld"
        flat_script.write_text("ENTRY(_start)\nSECTIONS { . = 0; "
                               ".text : { *(.text*) } .rodata : { *(.rodata*) } "
                               ".data : { *(.data*) } .bss : { *(.bss*) } }\n")

        def assemble(source, destination):
            run([cc, "--target=tms9900", "-c", source, "-o", destination])

        def compile_c(source, destination, opt, extra=()):
            run([cc, "--target=tms9900", "-ffreestanding", "-fno-builtin",
                 "-O" + opt, *extra, "-c", source, "-o", destination])

        def execute(name, objects, script, load="0x0000", entry="0x0000", poison_bss=False):
            nonlocal count
            elf, binary = out / "test.elf", out / "test.bin"
            run([ld, "-T", script, *objects, "-o", elf])
            run([objcopy, "-O", "binary", elf, binary])
            if poison_bss:
                symbols = {}
                for line in run([args.llvm_bin / "llvm-nm", elf]).splitlines():
                    fields = line.split()
                    if len(fields) == 3 and fields[2] in ("__bss_start", "__bss_end"):
                        symbols[fields[2]] = int(fields[0], 16)
                begin = symbols["__bss_start"] - int(load, 16)
                end = symbols["__bss_end"] - int(load, 16)
                image = bytearray(binary.read_bytes())
                if len(image) <= end:
                    image.extend(bytes(end + 1 - len(image)))
                image[begin:end] = bytes([0xcc]) * (end - begin)
                image[end] = 0x3c
                binary.write_bytes(image)
            step_limit = "1000000" if poison_bss else "50000000"
            lines = run([args.trace, "-S", "-l", load, "-e", entry,
                         "--no-loop-detect", "-n", step_limit, binary]).splitlines()
            state = json.loads(lines[-1])
            result = int(state["r"][0], 16)
            passed = state["halt"] == "idle" and result == 0
            count += 1
            print(f"{name}: {'PASS' if passed else 'FAIL'} "
                  f"(R0={result}, halt={state['halt']}, steps={state['steps']})",
                  flush=True)
            if not passed:
                failures.append(name)

        start_obj, wrapper_obj = out / "start.o", out / "main.o"
        assemble(start, start_obj)
        assemble(main_wrapper, wrapper_obj)

        if args.filter in (None, "crt"):
            # BSS fits the old 24 KiB maps but would overlap the reserved stack.
            fixture = out / "overflow.s"
            fixture.write_text(".text\n.globl check_startup\ncheck_startup:\nB *R11\n"
                               ".bss\n.space 0x5801\n")
            assemble(fixture, out / "overflow.o")
            assemble(args.crt, out / "crt.o")
            objects = [out / "crt.o", wrapper_obj, out / "overflow.o"]
            for script in [ROOT / "cart_example/cart.ld", ROOT / "cart_example/ea5.ld"]:
                result = subprocess.run([str(ld), "-T", str(script),
                                         *[str(obj) for obj in objects],
                                         "-o", str(out / "overflow.elf")], text=True,
                                        stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                passed = result.returncode != 0 and "region 'RAM'" in result.stderr and \
                    "overflowed" in result.stderr
                name = f"{script.name} rejects stack overlap"
                count += 1
                print(f"{name}: {'PASS' if passed else 'FAIL'}", flush=True)
                if not passed:
                    failures.append(name)

            fixture.write_text(".text\n.globl check_startup\ncheck_startup:\nB *R11\n")
            assemble(fixture, out / "small.o")
            for script in [ROOT / "cart_example/cart.ld", ROOT / "cart_example/ea5.ld"]:
                elf = out / "custom-stack.elf"
                run([ld, "--defsym=__stack_size=0x1000", "-T", script,
                     out / "crt.o", wrapper_obj, out / "small.o", "-o", elf])
                symbols = {fields[2]: int(fields[0], 16)
                           for line in run([args.llvm_bin / "llvm-nm", elf]).splitlines()
                           if len(fields := line.split()) == 3 and fields[1].lower() != "u"}
                passed = symbols["__heap_end"] == 0xf000 and \
                    symbols["__stack_top"] == 0xfffc and symbols["__heap_start"] % 2 == 0
                name = f"{script.name} configurable stack/heap boundary"
                count += 1
                print(f"{name}: {'PASS' if passed else 'FAIL'}", flush=True)
                if not passed:
                    failures.append(name)

        for opt in args.opt:
            if args.filter in (None, "peephole"):
                test_obj, kernel_obj = out / "test.o", out / "kernel.o"
                compile_c(HERE / "peephole.c", test_obj, opt)
                run([args.llc or args.llvm_bin / "llc", "-mtriple=tms9900", "-verify-machineinstrs",
                     "-start-before=tms9900-peephole", "-filetype=obj",
                     HERE / "peephole.mir", "-o", kernel_obj])
                execute(f"peephole -O{opt}", [start_obj, test_obj, kernel_obj], flat_script)

            if args.filter in (None, "memset"):
                test_obj, library_obj = out / "test.o", out / "library.o"
                compile_c(HERE / "memset.c", test_obj, opt)
                assemble(ROOT / "libtms9900/builtins/memset.S", library_obj)
                execute(f"memset -O{opt}", [start_obj, test_obj, library_obj], flat_script)

            if args.filter in (None, "malloc"):
                test_obj, library_obj = out / "test.o", out / "library.o"
                compile_c(HERE / "malloc.c", test_obj, opt)
                compile_c(ROOT / "libtms9900/builtins/malloc.c", library_obj, opt)
                for begin, end in [(0xa000, 0xa100), (0xa001, 0xa100),
                                   (0xff00, 0xffe0)]:
                    script = out / "heap.ld"
                    script.write_text("ENTRY(_start)\nSECTIONS { . = 0; "
                                      ".text : { *(.text*) } .rodata : { *(.rodata*) } "
                                      ".data : { *(.data*) } .bss : { *(.bss*) } "
                                      f"__heap_start = {begin}; __heap_end = {end}; }}\n")
                    execute(f"malloc -O{opt} heap={begin:04x}-{end:04x}",
                            [start_obj, test_obj, library_obj], script)

            if args.filter in (None, "crt"):
                crt_obj, test_obj = out / "crt.o", out / "test.o"
                assemble(args.crt, crt_obj)
                for size in [0, 1, 2, 3]:
                    compile_c(HERE / "crt_init.c", test_obj, opt,
                              [f"-DSECTION_BYTES={size}"])
                    objects = [crt_obj, wrapper_obj, test_obj]
                    execute(f"cart CRT -O{opt} bytes={size}", objects,
                            ROOT / "cart_example/cart.ld", "0x6000", "0x6024", True)
                    execute(f"EA5 CRT -O{opt} bytes={size}", objects,
                            ROOT / "cart_example/ea5.ld", "0xa000", "0xa000", True)

    print(f"{count - len(failures)}/{count} runtime tests passed")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
