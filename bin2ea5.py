#!/usr/bin/env python3

import argparse
import os


def parse_addr(value):
    if value.startswith(">"):
        return int(value[1:], 16)
    return int(value, 0)


def main():
    parser = argparse.ArgumentParser(
        description="Wrap a flat RAM binary into an EA5 program image (.img)."
    )
    parser.add_argument("binfile", help="Input RAM binary (.bin)")
    parser.add_argument("-o", "--output", help="Output .img path")
    parser.add_argument(
        "-a",
        "--base",
        type=parse_addr,
        default=0xA000,
        help="Load address (default >A000)",
    )
    parser.add_argument(
        "--max-size",
        type=parse_addr,
        default=0x1FFA,
        help="Max payload per image (default 0x1FFA = 0x2000-6)",
    )
    args = parser.parse_args()

    base = args.base
    max_size = args.max_size
    if not 0 <= base <= 0xFFFE or base & 1:
        parser.error("load address must be an even 16-bit address")
    if not 1 <= max_size <= 0xFFF9:
        parser.error("maximum payload must be between 1 and 65529 bytes")

    with open(args.binfile, "rb") as f:
        data = f.read()

    if not data:
        raise SystemExit("empty input")
    if len(data) > max_size:
        raise SystemExit(
            f"input too large ({len(data)} bytes), max {max_size}; split into multiple images"
        )
    if base + len(data) > 0x10000:
        parser.error("payload extends past the 16-bit address space")

    header = (
        b"\x00\x00" +
        ((len(data) + 6).to_bytes(2, "big")) +
        base.to_bytes(2, "big")
    )

    output = args.output or (os.path.splitext(args.binfile)[0] + ".img")
    with open(output, "wb") as f:
        f.write(header)
        f.write(data)


if __name__ == "__main__":
    main()
