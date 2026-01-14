#!/usr/bin/env python3
import argparse
import pathlib
import re
import subprocess
import sys
from typing import Dict, Tuple

ROOT = pathlib.Path(__file__).resolve().parents[1]
LLVM_BIN = ROOT / "llvm-project" / "build" / "bin"
CLANG = LLVM_BIN / "clang"
LD = LLVM_BIN / "ld.lld"
OBJDUMP = LLVM_BIN / "llvm-objdump"
SIZE = LLVM_BIN / "llvm-size"
OUT_DIR = pathlib.Path("/tmp/peephole_bench")
SRC = ROOT / "tests" / "peephole_bench.c"

MNEMONICS = ["AI", "INC", "INCT", "DEC", "DECT", "CLR", "XOR", "MOV", "MOVB"]


def run_cmd(cmd, cwd=None) -> str:
  return subprocess.check_output(cmd, cwd=cwd, text=True)


def parse_text_size(output: str) -> int:
  for line in output.splitlines():
    parts = line.strip().split()
    if len(parts) >= 2 and parts[0] == ".text":
      return int(parts[1])
  return 0


def parse_disasm(output: str) -> Tuple[int, Dict[str, int], int, int]:
  instr_count = 0
  counts = {m: 0 for m in MNEMONICS}
  mov_post = 0
  movb_post = 0

  for line in output.splitlines():
    m = re.match(r"^\s*[0-9a-f]+:\s+([0-9a-f]{2} )+\s+([A-Z]+)", line)
    if not m:
      continue
    instr_count += 1
    mnemonic = m.group(2)
    if mnemonic in counts:
      counts[mnemonic] += 1
    if mnemonic == "MOV" and "*R" in line and "+" in line:
      mov_post += 1
    if mnemonic == "MOVB" and "*R" in line and "+" in line:
      movb_post += 1

  return instr_count, counts, mov_post, movb_post


def build_obj(opt: str, disable: bool) -> pathlib.Path:
  OUT_DIR.mkdir(parents=True, exist_ok=True)
  tag = "off" if disable else "on"
  obj = OUT_DIR / f"peephole_{opt}_{tag}.o"
  cmd = [
      str(CLANG),
      "--target=tms9900",
      f"-O{opt}",
      "-fno-builtin",
      "-ffreestanding",
      "-c",
      str(SRC),
      "-o",
      str(obj),
  ]
  if disable:
    cmd += ["-mllvm", "-tms9900-disable-peephole"]
  run_cmd(cmd)
  return obj


def measure(obj: pathlib.Path) -> Tuple[int, int, Dict[str, int], int, int]:
  size_out = run_cmd([str(SIZE), "--format=SysV", "-A", str(obj)])
  text_size = parse_text_size(size_out)
  disasm = run_cmd([str(OBJDUMP), "-d", str(obj)])
  instr_count, counts, mov_post, movb_post = parse_disasm(disasm)
  return text_size, instr_count, counts, mov_post, movb_post


def build_ball2(opt: str, disable: bool) -> Tuple[pathlib.Path, pathlib.Path]:
  out = OUT_DIR / "ball2"
  out.mkdir(parents=True, exist_ok=True)
  tag = "off" if disable else "on"
  obj = out / f"ball2_{opt}_{tag}.o"
  crt = out / f"crt0_{tag}.o"
  elf = out / f"ball2_{opt}_{tag}.elf"
  bin_path = out / f"ball2_{opt}_{tag}.bin"
  cart_dir = ROOT / "cart_example"
  cart_ld = cart_dir / "cart.ld"

  run_cmd([
      str(CLANG),
      "--target=tms9900",
      "-c",
      str(cart_dir / "crt0.s"),
      "-o",
      str(crt),
  ])

  cmd = [
      str(CLANG),
      "--target=tms9900",
      f"-O{opt}",
      "-fno-builtin",
      "-ffreestanding",
      "-Wno-main-return-type",
      "-c",
      str(cart_dir / "ball2.c"),
      "-o",
      str(obj),
  ]
  if disable:
    cmd += ["-mllvm", "-tms9900-disable-peephole"]
  run_cmd(cmd)

  run_cmd([
      str(LD),
      "-T",
      str(cart_ld),
      str(crt),
      str(obj),
      "-o",
      str(elf),
  ])

  run_cmd([
      str(LLVM_BIN / "llvm-objcopy"),
      "-O",
      "binary",
      "--only-section=.cart_header",
      "--only-section=.cart_entry",
      "--only-section=.text",
      "--only-section=.rodata",
      "--only-section=.data",
      str(elf),
      str(bin_path),
  ])

  return elf, bin_path


def measure_elf(elf: pathlib.Path) -> Tuple[int, int, Dict[str, int], int, int]:
  size_out = run_cmd([str(SIZE), "--format=SysV", "-A", str(elf)])
  text_size = parse_text_size(size_out)
  disasm = run_cmd([str(OBJDUMP), "-d", str(elf)])
  instr_count, counts, mov_post, movb_post = parse_disasm(disasm)
  return text_size, instr_count, counts, mov_post, movb_post


def print_counts(label: str, text_size: int, instrs: int,
                 counts: Dict[str, int], mov_post: int, movb_post: int) -> None:
  print(f"{label}:")
  print(f"  .text bytes: {text_size}")
  print(f"  instructions: {instrs}")
  print(f"  MOV *R+ count: {mov_post}")
  print(f"  MOVB *R+ count: {movb_post}")
  print("  mnemonics:")
  for key in MNEMONICS:
    print(f"    {key}: {counts.get(key, 0)}")


def main() -> int:
  if not CLANG.exists():
    print(f"Missing clang at {CLANG}", file=sys.stderr)
    return 2
  if not OBJDUMP.exists() or not SIZE.exists():
    print("Missing llvm-objdump or llvm-size", file=sys.stderr)
    return 2

  parser = argparse.ArgumentParser(description="Measure TMS9900 peephole wins")
  parser.add_argument("--opt", default="2", help="Optimization level (0,1,2,3,s)")
  parser.add_argument("--ball2", action="store_true",
                      help="Measure ball2.c (cart_example) instead of microbench")
  args = parser.parse_args()
  opt = args.opt

  if args.ball2:
    elf_off, bin_off = build_ball2(opt, disable=True)
    elf_on, bin_on = build_ball2(opt, disable=False)
    off = measure_elf(elf_off)
    on = measure_elf(elf_on)
    bin_off_size = bin_off.stat().st_size
    bin_on_size = bin_on.stat().st_size
  else:
    obj_off = build_obj(opt, disable=True)
    obj_on = build_obj(opt, disable=False)
    off = measure(obj_off)
    on = measure(obj_on)
    bin_off_size = 0
    bin_on_size = 0

  print_counts("peephole off", *off)
  print_counts("peephole on", *on)

  off_size, off_instrs, _, _, _ = off
  on_size, on_instrs, _, _, _ = on

  if off_size:
    delta_size = on_size - off_size
    pct_size = (delta_size / off_size) * 100
    print(f"size delta: {delta_size} bytes ({pct_size:+.2f}%)")
  if off_instrs:
    delta_instr = on_instrs - off_instrs
    pct_instr = (delta_instr / off_instrs) * 100
    print(f"instruction delta: {delta_instr} ({pct_instr:+.2f}%)")
  if bin_off_size:
    delta_bin = bin_on_size - bin_off_size
    pct_bin = (delta_bin / bin_off_size) * 100
    print(f"bin size delta: {delta_bin} bytes ({pct_bin:+.2f}%)")
  return 0


if __name__ == "__main__":
  raise SystemExit(main())
