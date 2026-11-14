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
    parser.add_argument("--build-all", action="store_true",
                        help="Also build and verify every distributed format with the demo defaults")
    args = parser.parse_args()
    tools = args.llvm_bin.resolve()
    failures = []
    count = 0
    with tempfile.TemporaryDirectory(prefix="tms9900-demo-startup-") as directory:
        root = Path(directory)
        demos = root / "cart_example"
        demos.mkdir()
        (root / "tools").symlink_to(ROOT / "tools", target_is_directory=True)
        for pattern in ["Makefile", "*.ld", "*.c", "*.h", "*.s", "*.S"]:
            for source in args.demo_dir.glob(pattern):
                shutil.copy2(source, demos / source.name)
        for name in ["bin2ea5.py", "bin2rpk.py"]:
            shutil.copy2(ROOT / name, root / name)
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
        if args.build_all:
            import zipfile
            import xml.etree.ElementTree as ET
            run(["make", "clean", f"LLVM_BUILD={tools}"], cwd=demos)
            run(["make", "-j4", "all", f"LLVM_BUILD={tools}"], cwd=demos)
            bins = sorted(demos.glob("*.bin"))
            for binary in bins:
                if binary.name.endswith(".ea5.bin"):
                    continue
                payload = binary.read_bytes()
                cart = binary.with_suffix(".cart").read_bytes()
                with zipfile.ZipFile(binary.with_suffix(".rpk")) as archive:
                    layout = ET.fromstring(archive.read("layout.xml"))
                    rom = archive.read(layout.find("resources/rom").get("file"))
                ea5 = binary.with_suffix(".img").read_bytes()
                ram_payload = binary.with_suffix(".ea5.bin").read_bytes()
                passed = (0 < len(payload) <= 8192 and payload == cart == rom
                          and ea5[:2] == b"\0\0"
                          and int.from_bytes(ea5[2:4], "big") == len(ea5)
                          and ea5[4:6] == b"\xa0\0" and ea5[6:] == ram_payload)
                symbols = {fields[2]: int(fields[0], 16)
                           for line in run([tools / "llvm-nm", binary.with_suffix(".elf")]).splitlines()
                           if len(fields := line.split()) == 3 and fields[1].lower() != "u"}
                entry = int.from_bytes(payload[0x12:0x14], "big")
                passed = passed and entry == symbols["_start"]
                if binary.stem == "life3_2x":
                    passed = passed and payload[0x14] == 16 and payload[0x15:0x25] == b"GAME OF LIFE 0.2"
                else:
                    passed = passed and entry == 0x6024
                count += 1
                print(f"{binary.stem} distribution: {'PASS' if passed else 'FAIL'} "
                      f"({len(payload)} ROM bytes)", flush=True)
                if not passed:
                    failures.append(binary.stem + " distribution")
    print(f"{count - len(failures)}/{count} demo startup tests passed")
    return bool(failures)


if __name__ == "__main__":
    raise SystemExit(main())
