#!/usr/bin/env python3

import argparse
import os
import zipfile
import xml.etree.ElementTree as ET


def parse_header_name(data):
    if len(data) < 16:
        return None
    if data[0] != 0xAA or data[1] != 0x01:
        return None
    # The program-list pointer is an address in the cartridge's >6000 window;
    # the first program need not be at the demos' usual >6010 offset.
    entry = int.from_bytes(data[6:8], "big") - 0x6000
    if entry < 0 or entry + 5 > len(data):
        return None
    name_len = data[entry + 4]
    if name_len == 0:
        return None
    start = entry + 5
    end = start + name_len
    if end > len(data):
        return None
    try:
        return data[start:end].decode("ascii")
    except UnicodeDecodeError:
        return None


def guess_pcb(size):
    return "paged378" if size > 0x2000 else "standard"


def main():
    parser = argparse.ArgumentParser(description="Wrap TI-99/4A ROM .bin into a MAME .rpk.")
    parser.add_argument("binfile", help="Input ROM binary (.bin)")
    parser.add_argument("-o", "--output", help="Output .rpk path")
    parser.add_argument("-n", "--name", help="Cartridge display name in metadata")
    parser.add_argument("--pcb", choices=["standard", "paged378"], help="Override PCB type")
    args = parser.parse_args()

    with open(args.binfile, "rb") as f:
        data = f.read()

    if not data:
        raise SystemExit("empty input")

    base = os.path.splitext(os.path.basename(args.binfile))[0]
    header_name = parse_header_name(data)
    name = args.name or header_name or base

    if any(not (0x20 <= ord(c) <= 0xD7FF or 0xE000 <= ord(c) <= 0xFFFD
               or 0x10000 <= ord(c) <= 0x10FFFF) for c in name):
        parser.error("cartridge name contains an invalid XML character")

    pcb = args.pcb or guess_pcb(len(data))
    output = args.output or (base + ".rpk")

    # Keep the ZIP resource name independent of display text (quotes, slashes,
    # etc.), and let XML serialization escape the metadata correctly.
    rom_file = "rom.bin"
    layout = ET.Element("romset", version="1.0")
    resources = ET.SubElement(layout, "resources")
    ET.SubElement(resources, "rom", id="romimage", file=rom_file)
    config = ET.SubElement(layout, "configuration")
    board = ET.SubElement(config, "pcb", type=pcb)
    ET.SubElement(board, "socket", id="rom_socket", uses="romimage")
    metainf = ET.Element("meta-inf")
    ET.SubElement(metainf, "name").text = name

    with zipfile.ZipFile(output, "w") as archive:
        archive.writestr(rom_file, data)
        archive.writestr("layout.xml", ET.tostring(layout, encoding="utf-8", xml_declaration=True))
        archive.writestr("meta-inf.xml", ET.tostring(metainf, encoding="utf-8", xml_declaration=True))


if __name__ == "__main__":
    main()
