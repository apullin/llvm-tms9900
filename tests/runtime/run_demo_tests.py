#!/usr/bin/env python3
"""Check real demo startup without requiring VDP or TI console emulation."""

import argparse
import json
from pathlib import Path
import shutil
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
    parser.add_argument("--demo-dir", type=Path, default=ROOT / "cart_example",
                        help="Override sources/Makefile for before/after comparisons")
    parser.add_argument("--opt", nargs="+", choices=["0", "1", "2", "3", "s", "z"],
                        default=["0", "1", "2", "3", "s", "z"])
    args = parser.parse_args()
    tools = args.llvm_bin.resolve()
    failures = []
    count = 0
    with tempfile.TemporaryDirectory(prefix="tms9900-demo-startup-") as directory:
        root = Path(directory)
        demos = root / "cart_example"
        demos.mkdir()
        (root / "tools").symlink_to(ROOT / "tools", target_is_directory=True)
        for name in ["Makefile", "cart.ld", "ea5.ld", "crt0.s", "banner.c", "banner2.c"]:
            shutil.copy2(args.demo_dir / name, demos / name)
        for opt in args.opt:
            run(["make", "clean", f"LLVM_BUILD={tools}"], cwd=demos)
            flags = (f"--target=tms9900 -O{opt} -ffreestanding -fno-builtin "
                     "-Wno-main-return-type")
            for demo in ["banner", "banner2"]:
                for mode, base in [("", 0x6000), (".ea5", 0xa000)]:
                    elf = demos / f"{demo}{mode}.elf"
                    run(["make", elf.name, f"LLVM_BUILD={tools}", f"CFLAGS={flags}"], cwd=demos)
                    symbols = {fields[2]: int(fields[0], 16)
                               for line in run([tools / "llvm-nm", elf]).splitlines()
                               if len(fields := line.split()) == 3 and fields[1].lower() != "u"}
                    binary = demos / "test.bin"
                    run([tools / "llvm-objcopy", "-O", "binary", elf, binary])
                    state = json.loads(run([args.trace.resolve(), "-S", "--no-loop-detect",
                                            "-l", hex(base), "-e", hex(symbols["_start"]),
                                            "-s", hex(symbols["main"]), "-n", "10000", binary]))
                    passed = (state["halt"] == "stop" and int(state["pc"], 16) == symbols["main"]
                              and int(state["wp"], 16) == 0x8300
                              and int(state["r"][10], 16) == symbols["__stack_top"] == 0xfffc
                              and int(state["st"], 16) & 15 == 0)
                    name = f"{demo} {mode or 'cart'} -O{opt} entry"
                    count += 1
                    print(f"{name}: {'PASS' if passed else 'FAIL'} "
                          f"(halt={state['halt']}, SP={state['r'][10]})", flush=True)
                    if not passed:
                        failures.append(name)
    print(f"{count - len(failures)}/{count} demo startup tests passed")
    return bool(failures)


if __name__ == "__main__":
    raise SystemExit(main())
