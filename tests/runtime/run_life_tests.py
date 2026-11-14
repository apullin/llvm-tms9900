#!/usr/bin/env python3
"""Execute the shipped Life code and verify its board and VDP output on the CPU."""

import argparse
import json
from pathlib import Path
import random
import re
import statistics
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
WIDTH, HEIGHT = 128, 96
ROW_BYTES, TILE_ROWS, TILE_COLS = 16, 24, 32
BOARD_BYTES = HEIGHT * ROW_BYTES


def run(command):
    result = subprocess.run([str(arg) for arg in command], text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if result.returncode:
        raise RuntimeError(f"{command}\n{result.stdout}{result.stderr}")
    return result.stdout


def check(condition, message):
    if not condition:
        raise RuntimeError(message)


def pack(cells):
    result = bytearray(BOARD_BYTES)
    for y, x in cells:
        result[y * ROW_BYTES + x // 8] |= 0x80 >> (x % 8)
    return bytes(result)


def unpack(board):
    return {(y, x) for y in range(HEIGHT) for x in range(WIDTH)
            if board[y * ROW_BYTES + x // 8] & (0x80 >> (x % 8))}


def generation(cells):
    # A scalar neighbor count, independent of the target's bit-parallel adder.
    counts = {}
    for y, x in cells:
        for dy in (-1, 0, 1):
            for dx in (-1, 0, 1):
                if dx or dy:
                    key = ((y + dy) % HEIGHT, (x + dx) % WIDTH)
                    counts[key] = counts.get(key, 0) + 1
    return {cell for cell, count in counts.items()
            if count == 3 or count == 2 and cell in cells}


def dirty_map(before, after):
    result = bytearray(TILE_ROWS * TILE_COLS)
    for y, x in before ^ after:
        result[(y // 4) * TILE_COLS + x // 4] = 1
    return bytes(result)


def activity(cells, rows):
    result = bytearray(TILE_ROWS if rows else TILE_ROWS * TILE_COLS)
    for y, x in cells:
        ty, tx = y // 4, x // 4
        for dy in (-1, 0, 1):
            ay = (ty + dy) % TILE_ROWS
            if rows:
                result[ay] = 1
            else:
                for dx in (-1, 0, 1):
                    result[ay * TILE_COLS + (tx + dx) % TILE_COLS] = 1
    return bytes(result)


def initial_seed(source):
    # Reproduce the documented RNG/block/dropout contract in cell coordinates.
    # Compare this with actual main/startup, where seeding can be inlined.
    def setting(name):
        return int(re.search(rf"^#define {name} (\d+)\s*$", source, re.M).group(1))

    state = int(re.search(r"rng_state = (0x[0-9A-Fa-f]+)u", source).group(1), 16)
    block = setting("LIFE_SEED_BLOCK_SIZE")
    threshold = setting("LIFE_SEED_THRESHOLD")
    dropout = setting("LIFE_SEED_DROPOUT_PCT") * 256 // 100
    cells = set()

    def rng():
        nonlocal state
        state = (state >> 1) ^ (0xb400 if state & 1 else 0)
        return state

    def seed_block(x, y):
        for dy in range(block):
            for dx in range(block):
                if dropout and (rng() & 255) < dropout:
                    continue
                cells.add(((y + dy) % HEIGHT, (x + dx) % WIDTH))

    x_offset, y_offset = rng() % block, rng() % block
    for y in range(0, HEIGHT, block):
        for x in range(0, WIDTH, block):
            if (rng() & 255) < threshold:
                seed_block(x + x_offset, y + y_offset)
    if setting("LIFE_SEED_EDGE_BLOCKS"):
        edge_x = (rng() & 127) // block * block
        edge_y = rng() & 127
        while edge_y >= HEIGHT:
            edge_y = rng() & 127
        edge_y = edge_y // block * block
        for x, y in [(0, edge_y), (WIDTH - block, edge_y),
                     (edge_x, 0), (edge_x, HEIGHT - block)]:
            seed_block(x, y)
    return cells, state


def interactive_addition(cells, state, kind, blob_geometry=None):
    """Independent cell-coordinate model of the additive N/M controls."""
    result = set(cells)

    def byte():
        nonlocal state
        for _ in range(8):
            state = (state >> 1) ^ (0xb400 if state & 1 else 0)
        return state & 255

    def below(bound):
        mask = (1 << (bound - 1).bit_length()) - 1
        value = byte() & mask
        while value >= bound:
            value = byte() & mask
        return value

    if kind == "N":
        for _ in range(WIDTH * HEIGHT // 32):
            x, y = below(WIDTH), below(HEIGHT)
            result.add((y, x))
    else:
        x0, y0 = below(WIDTH), below(HEIGHT)
        radius = 12 + below(12)
        if blob_geometry is not None:
            blob_geometry.append((x0, y0, radius))
        for dy in range(-radius, radius + 1):
            for dx in range(-radius, radius + 1):
                if dx * dx + dy * dy <= radius * radius and byte() & 128:
                    result.add(((y0 + dy) % HEIGHT, (x0 + dx) % WIDTH))
    return result, state


class Runner:
    def __init__(self, tools, trace, demos, directory):
        self.tools, self.trace, self.demos, self.directory = tools, trace, demos, directory

    def compile(self, source, output, opt="2", sections=False):
        flags = ["--target=tms9900"]
        if source.suffix == ".c":
            flags += [f"-O{opt}", "-ffreestanding", "-fno-builtin", "-Wno-main-return-type"]
            if sections:
                flags += ["-ffunction-sections", "-fdata-sections"]
        run([self.tools / "clang", *flags, "-c", source, "-o", output])

    def symbols(self, elf):
        return {fields[2]: int(fields[0], 16)
                for line in run([self.tools / "llvm-nm", elf]).splitlines()
                if len(fields := line.split()) == 3 and fields[1].lower() != "u"}

    def binary(self, elf, output, cart=False):
        flags = []
        if cart:
            flags = [f"--only-section={section}" for section in
                     [".cart_header", ".cart_entry", ".text", ".rodata", ".data"]]
        run([self.tools / "llvm-objcopy", "-O", "binary", *flags, elf, output])
        return output.read_bytes()

    def probe(self, directory, assembly, address=0x2000, objects=()):
        source = directory / "probe.s"
        source.write_text(assembly)
        self.compile(source, directory / "probe.o")
        elf = directory / "probe.elf"
        run([self.tools / "ld.lld", f"-Ttext={hex(address)}", "--gc-sections",
             directory / "probe.o", *objects, "-o", elf])
        return elf, self.binary(elf, directory / "probe-code.bin")

    def simulate(self, directory, image, entry, dumps=(), points=(), extra=()):
        binary = directory / "probe-image.bin"
        binary.write_bytes(image)
        arguments = [self.trace, "-q", "-S", "--no-loop-detect", "-n", "100000000",
                     "-l", "0x0", "-e", hex(entry)]
        for start, length in dumps:
            arguments += ["-d", f"{hex(start)}:{hex(length)}"]
        for point in points:
            arguments += ["--tracepoint", hex(point)]
        result = subprocess.run([*map(str, arguments), *map(str, extra), str(binary)],
                                text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        check(result.returncode == 0, result.stdout + result.stderr)
        records = [json.loads(line) for line in result.stdout.splitlines()]
        memory = bytearray(65536)
        for line in result.stderr.splitlines():
            if re.match(r"^\s*[0-9a-fA-F]{4}:", line):
                address, values = line.split("|")[0].split(":", 1)
                data = bytes.fromhex(values.strip())
                start = int(address, 16)
                memory[start:start + len(data)] = data
        return records, memory

    def seed_tests(self, opt):
        # Include each actual translation unit. Section GC removes the demo
        # main/VDP code while retaining the seeding functions used by wrappers.
        origins = [(127, 95), (126, 94), (127, 0), (0, 95), (0, 0), (124, 92)]
        for name in ["life2x", "life2_2x", "life3_2x"]:
            directory = self.directory / f"seed-{name}-O{opt}"
            directory.mkdir()
            source = directory / "seed.c"
            source.write_text(f'''#define main life_demo_main
#include "{self.demos / (name + '.c')}"
#undef main
void test_seed(uint16_t x, uint16_t y) {{
    seed_block((uint8_t (*)[CELL_ROW_BYTES])0xa100, x, y, 0);
}}
void test_rng(void) {{
    volatile uint8_t *values = (volatile uint8_t *)0xa100;
    uint16_t i;
    for (i = 0; i < 8192; ++i) values[i] = (uint8_t)rng_below(CELL_H);
}}
''')
            self.compile(source, directory / "seed.o", opt, sections=True)
            for x, y in origins:
                _, code = self.probe(directory, f'''.text
.globl _start
_start:
  LWPI 0xf000
  LI R10,0xfffc
  LI R13,0xd013
  LI R14,0xd014
  LI R15,0xd015
  LI R0,{x}
  LI R1,{y}
  BL @test_seed
  IDLE
''', objects=[directory / "seed.o"])
                image = bytearray(65536)
                image[0x2000:0x2000 + len(code)] = code
                image[0xa0c0:0xa100] = b"\xa5" * 64
                image[0xa700:0xa740] = b"\x5a" * 64
                records, memory = self.simulate(directory, image, 0x2000, [(0xa0c0, 0x680)])
                check(records[-1]["halt"] == "idle", f"{name} seed did not halt")
                registers = [int(value, 16) for value in records[-1]["r"]]
                check(registers[10] == 0xfffc and registers[13:16] == [0xd013, 0xd014, 0xd015],
                      f"{name} seed ABI mismatch")
                expected = pack({((y + dy) % HEIGHT, (x + dx) % WIDTH)
                                 for dy in range(4) for dx in range(4)})
                check(memory[0xa100:0xa700] == expected, f"{name} -O{opt} seed {x},{y} mismatch")
                check(memory[0xa0c0:0xa100] == image[0xa0c0:0xa100] and
                      memory[0xa700:0xa740] == image[0xa700:0xa740], f"{name} seed overrun")
            elf, code = self.probe(directory, '''.text
.globl _start
_start:
  LWPI 0xf000
  LI R10,0xfffc
  BL @test_rng
  IDLE
''', objects=[directory / "seed.o"])
            image = bytearray(65536)
            image[0x2000:0x2000 + len(code)] = code
            image[0xc100:0xc140] = b"\xa5" * 64
            records, memory = self.simulate(directory, image, 0x2000, [(0xa100, 0x2040)])
            check(records[-1]["halt"] == "idle", f"{name} RNG did not halt")
            values = memory[0xa100:0xc100]
            check(set(values) == set(range(HEIGHT)), f"{name} RNG misses rows or exceeds bounds")
            check(memory[0xc100:0xc140] == image[0xc100:0xc140], f"{name} RNG overrun")
            histogram = [values.count(row) for row in range(HEIGHT)]
            check(max(histogram) < 2 * min(histogram), f"{name} RNG has gross row bias")
            print(f"{name} -O{opt}: six toroidal seed/guard cases and 8192 bounded RNG samples PASS",
                  flush=True)

    def build_cart(self, opt, name="life3_2x", snapshot=False):
        directory = self.directory / f"cart-{name}-O{opt}{'-snapshot' if snapshot else ''}"
        directory.mkdir()
        paths = [self.demos / "crt0.s", self.demos / (name + ".c")]
        if snapshot:
            keyboard = directory / "keyboard.s"
            if name == "life3_2x":
                # SPACE dismisses the splash; subsequent scans request F.
                keyboard.write_text('''.bss
.balign 2
.Lscans: .short 0
.text
.globl kbd_scan
kbd_scan:
 INC @.Lscans
 MOV @.Lscans,R0
 CI R0,1
 JNE .Lf
 LI R0,32
 B *R11
.Lf:
 LI R0,70
 B *R11
''')
            else:
                keyboard.write_text(".text\n.globl kbd_scan\nkbd_scan:\n LI R0,70\n B *R11\n")
            paths.append(keyboard)
        else:
            paths.append(self.demos / "kbd.S")
        if name == "life3_2x":
            paths.append(self.demos / "life3_2x_step.S")
        objects = []
        for index, source in enumerate(paths):
            obj = directory / f"source-{index}.o"
            self.compile(source, obj, opt)
            objects.append(obj)
        elf = directory / "cart.elf"
        # Test oversized Life3 levels with the shipped EA5 map. Only the
        # physical ROM bound triggers this fallback; other link failures fail.
        link = [self.tools / "ld.lld", "--defsym=__stack_size=0x800"]
        try:
            run([*link, "-T", self.demos / "cart.ld", *objects, "-o", elf])
        except RuntimeError as error:
            if name != "life3_2x" or "will not fit in region 'ROM'" not in str(error):
                raise
            run([*link, "-T", self.demos / "ea5.ld", *objects, "-o", elf])
        rom = self.binary(elf, directory / "cart.bin", cart=True)
        return directory, self.symbols(elf), rom

    def row_tests(self):
        directory = self.directory / "row"
        directory.mkdir()
        self.compile(self.demos / "life3_2x_step.S", directory / "step.o")
        _, code = self.probe(directory, '''.text
.globl _start
_start:
  LWPI 0xf000
  LI R10,0xfffc
  LI R9,0x9123
  LI R13,0x100
  LI R14,0x9000
  LI R15,512
.Lcase:
  MOV R13,R0
  MOV R13,R1
  AI R1,16
  MOV R1,R2
  AI R2,16
  MOV R14,R3
  BL @life_compute_row
  AI R13,48
  AI R14,16
  DEC R15
  JNE .Lcase
  IDLE
''', address=0x7000, objects=[directory / "step.o"])
        for phase in range(8):
            fixtures, expected = bytearray(), bytearray()
            for pattern in range(512):
                rows = [[0] * WIDTH for _ in range(3)]
                for lane in range(16):
                    center = phase + lane * 8
                    for bit in range(9):
                        y, dx = divmod(bit, 3)
                        rows[y][(center + dx - 1) % WIDTH] = (pattern >> bit) & 1
                fixtures += bytes(sum(row[x + bit] << (7 - bit) for bit in range(8))
                                  for row in rows for x in range(0, WIDTH, 8))
                cells = {(y, x) for y in range(3) for x in range(WIDTH) if rows[y][x]}
                expected += pack(generation(cells))[ROW_BYTES:2 * ROW_BYTES]
            image = bytearray(65536)
            image[0x100:0x6100] = fixtures
            image[0x7000:0x7000 + len(code)] = code
            image[0x8ffc:0x9000] = b"\xa5\x3c" * 2
            image[0xb000:0xb004] = b"\x5a\xc3" * 2
            records, memory = self.simulate(directory, image, 0x7000,
                                           [(0x100, 0x6000), (0x8ffc, 0x2008)])
            check(records[-1]["halt"] == "idle", "row kernel did not halt")
            registers = [int(value, 16) for value in records[-1]["r"]]
            check(registers[9] == 0x9123 and registers[10] == 0xfffc and
                  registers[13:16] == [0x6100, 0xb000, 0], "row kernel ABI mismatch")
            check(memory[0x9000:0xb000] == expected, f"row phase {phase}: result mismatch")
            check(memory[0x100:0x6100] == fixtures, "row kernel modified source")
            for start, end in [(0x8ffc, 0x9000), (0xb000, 0xb004)]:
                check(memory[start:end] == image[start:end], "row kernel overwrote guard")
        print("row kernel: all 512 neighborhoods at 128 alignments, source/guards/ABI PASS", flush=True)

    def program_memory(self, sym, rom):
        image = bytearray(65536)
        base = sym.get("cart_header", sym["_start"])
        image[base:base + len(rom)] = rom
        return image

    def cart_memory(self, sym, rom):
        image = self.program_memory(sym, rom)
        size = sym["__data_end"] - sym["__data_start"]
        start = sym["__data_load_start"]
        image[sym["__data_start"]:sym["__data_end"]] = image[start:start + size]
        return image

    def keyboard_stub(self, directory, image, sym, keys=(0, 70, 32)):
        # Leave the shipping keyboard helper in the measured cart. The CPU
        # test image alone redirects the TI ROM boundary to scripted keys.
        # Repeat the final key; None halts the CPU for a bounded main-loop test.
        def answer(key):
            return " IDLE\n" if key is None else f" LI R0,{key}\n B *R11\n"

        assembly = ".text\n.globl _start\n_start:\n INC @0x0200\n MOV @0x0200,R0\n"
        for index in range(len(keys) - 1):
            assembly += f" CI R0,{index + 1}\n JEQ .Lkey{index}\n"
        assembly += answer(keys[-1])
        for index, key in enumerate(keys[:-1]):
            assembly += f".Lkey{index}:\n" + answer(key)
        assembly += ".globl keyboard_redirect\nkeyboard_redirect:\n B @_start\n"
        elf, code = self.probe(directory, assembly)
        image[0x2000:0x2000 + len(code)] = code
        redirect = self.symbols(elf)["keyboard_redirect"] - 0x2000
        image[sym["kbd_scan"]:sym["kbd_scan"] + 4] = code[redirect:redirect + 4]

    def vdp_writes(self, directory):
        writes = {}
        disassembly = run([self.tools / "llvm-objdump", "-d", directory / "cart.elf"])
        for line in disassembly.splitlines():
            match = re.match(r"\s*([0-9a-f]+):.*\bMOVB\s+R(\d+),@0x(8c00|8c02)", line, re.I)
            if match:
                pc, register, port = match.groups()
                writes[int(pc, 16)] = int(register), int(port, 16)
        check(writes, "cannot find VDP byte writes")
        return writes

    def vdp_screen(self, records, writes):
        # Reconstruct the VDP protocol from actual MOVB high-byte values.
        vram, registers, register_changes = bytearray(0x4000), [0] * 8, []
        latch, address = None, None
        for event in records:
            pc = int(event["pc"], 16)
            if pc not in writes:
                continue
            register, port = writes[pc]
            value = int(event["r"][register], 16) >> 8
            if port == 0x8c02:
                if latch is None:
                    latch = value
                else:
                    if value & 0x80:
                        registers[value & 7] = latch
                        register_changes.append((value & 7, latch))
                    else:
                        check(value & 0x40, "unexpected VDP read request")
                        address = latch | ((value & 0x3f) << 8)
                    latch = None
            else:
                check(latch is None and address is not None, "VDP data without completed write address")
                vram[address] = value
                address = (address + 1) & 0x3fff
        check(latch is None, "incomplete VDP command")
        return vram, registers, register_changes

    def splash_tests(self, cart, opt, preview=None):
        directory, sym, rom = cart
        entry = sym["_start"]
        base = sym.get("cart_header", entry)
        if "cart_header" in sym:
            check(int.from_bytes(rom[0x12:0x14], "big") == entry == 0x6026 and
                  rom[0x14] == 16 and rom[0x15:0x25] == b"GAME OF LIFE 0.2",
                  "versioned menu entry/address mismatch")
        else:
            check(entry == 0xa000, "EA5 entry mismatch")
        image = self.program_memory(sym, rom)
        writes = self.vdp_writes(directory)
        records, _ = self.simulate(directory, image, entry, points=writes,
                                   extra=["-s", hex(sym["life_splash_ready"])])
        check(records[-1]["halt"] == "stop", "splash was not drawn before keyboard scan")
        vram, registers, changes = self.vdp_screen(records[:-1], writes)
        check(registers == [0, 0xe0, 6, 0x80, 0, 0x36, 7, 1], "splash VDP configuration mismatch")
        check(changes[0] == (1, 0x80) and changes[-1] == (1, 0xe0),
              "splash display was not blanked during setup")
        expected = bytearray(b" " * 768)
        headings = dict(A=[2, 5, 7, 5, 5], E=[7, 4, 6, 4, 7], F=[7, 4, 6, 4, 4],
                        G=[3, 4, 5, 5, 3], I=[7, 2, 2, 2, 7], L=[4, 4, 4, 4, 7],
                        M=[9, 15, 15, 9, 9], O=[2, 5, 5, 5, 2])
        for row, col, text in [(1, 2, "GAME OF"), (7, 8, "LIFE")]:
            for letter in text:
                columns = 4 if letter == "M" else 3
                if letter != " ":
                    for y, mask in enumerate(headings[letter]):
                        for x in range(columns):
                            if mask & (1 << (columns - x - 1)):
                                expected[(row + y) * 32 + col + x] = 0x7f
                col += columns + 1
        text_lines = [(13, 9, "V0.2  OCT 2026"), (15, 3, "ZXCVB   DENSITY"),
                      (16, 7, "R   RESTART"), (17, 5, "1-9   ADD RANDOM ENTROPY"),
                      (18, 7, "D   SHOW DIRTY TILES"), (19, 7, "F   SHOW CLEAN TILES"),
                      (20, 7, "N   ADD NOISE"), (21, 7, "M   ADD BLOB"),
                      (23, 9, "SPACE TO START")]
        for row, col, text in text_lines:
            expected[row * 32 + col:row * 32 + col + len(text)] = text.encode("ascii")
        check(vram[0x1800:0x1b00] == expected, "splash title/version/controls layout mismatch")
        check(vram[0x1b00] == 0xd0, "splash sprites were not disabled")
        colors = bytearray([0xf1] * 32)
        colors[15] = 0x71
        check(vram[0x2000:0x2020] == colors, "splash colors mismatch")
        check(vram[0x3f8:0x400] == b"\xff" * 8, "splash block pattern mismatch")
        for tile in set(expected) - {0x7f}:
            start = sym["splash_font"] + (tile - 32) * 7 - base
            pattern = bytes(value << 2 for value in rom[start:start + 7]) + b"\0"
            check(vram[tile * 8:tile * 8 + 8] == pattern, "splash font byte upload mismatch")
            check(tile == 32 or any(pattern), "splash text uses a blank/missing glyph")
        if preview:
            # Render the captured Graphics I VRAM, not a separate mockup.
            import struct
            import zlib
            palette = {1: (0, 0, 0), 7: (66, 235, 245), 15: (255, 255, 255)}
            rows = []
            for y in range(192):
                pixels = bytearray()
                for x in range(256):
                    tile = vram[0x1800 + (y // 8) * 32 + x // 8]
                    pattern = vram[tile * 8 + y % 8]
                    color = vram[0x2000 + tile // 8]
                    index = color >> 4 if pattern & (0x80 >> (x % 8)) else color & 15
                    pixels += bytes(palette[index]) * 3
                rows.extend([b"\0" + pixels] * 3)
            def chunk(kind, data):
                return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))
            preview.write_bytes(b"\x89PNG\r\n\x1a\n" +
                                chunk(b"IHDR", struct.pack(">2I5B", 768, 576, 8, 2, 0, 0, 0)) +
                                chunk(b"IDAT", zlib.compress(b"".join(rows))) + chunk(b"IEND", b""))
        self.keyboard_stub(directory, image, sym)
        # Stop before the third scan: neither no-key nor F may start the game.
        records, memory = self.simulate(directory, image, entry, [(0xa000, 0x5800)],
                                       [sym["kbd_scan"]], ["--tracepoint-max", "3"])
        check(records[-1]["halt"] == "tracepoint-max", "splash did not keep polling")
        address = sym["board_a"]
        check(memory[address:address + BOARD_BYTES] == bytes(BOARD_BYTES), "splash started on a non-SPACE key")
        records, _ = self.simulate(directory, image, entry, points=[*writes, sym["kbd_scan"]],
                                   extra=["-s", hex(sym["step_board"])])
        check(records[-1]["halt"] == "stop", "SPACE did not start the game")
        scans = sum(int(event["pc"], 16) == sym["kbd_scan"] for event in records[:-1])
        check(scans == 3, "SPACE gating did not follow the scripted key sequence")
        _, registers, changes = self.vdp_screen(records[:-1], writes)
        split = changes.index((1, 0xe0)) + 1
        check(changes[split:split + 2] == [(1, 0x80), (0, 2)], "mode changed with display still enabled")
        check(registers == [2, 0xe0, 6, 0xff, 3, 0x36, 7, 1], "game VDP configuration was not restored")
        mode = "cart" if "cart_header" in sym else "EA5"
        print(f"splash -O{opt} ({mode}): entry, captured font/layout/colors, SPACE gate and mode transition PASS", flush=True)

    def frame_tests(self, cart, opt, cases, frames, label="current"):
        directory, sym, rom = cart
        _, code = self.probe(directory, f'''.text
.globl _start
_start:
  LWPI 0x8300
  LI R10,0xfffc
  LI R13,{sym['board_a']}
  LI R14,{sym['board_b']}
  LI R15,{frames}
.Lframe:
  MOV R13,R0
  MOV R14,R1
  BL @{sym['step_board']}
  MOV R14,R0
  BL @{sym['vdp_write_dirty_patterns']}
  MOV R13,R0
  MOV R14,R13
  MOV R0,R14
  DEC R15
  JNE .Lframe
  IDLE
''')
        names = ["step_board", "life_step_end", "vdp_write_dirty_patterns", "life_draw_end"]
        points = [sym[name] for name in names]
        row_activity = "active_rows" in sym
        results = []
        for case, cells in cases.items():
            expected = set(cells)
            for _ in range(frames):
                previous = expected
                expected = generation(expected)
            image = self.cart_memory(sym, rom)
            image[0x2000:0x2000 + len(code)] = code
            image[sym["board_a"]:sym["board_a"] + BOARD_BYTES] = pack(cells)
            # Stale data deliberately exercises selective clearing of inactive rows.
            image[sym["board_b"]:sym["board_b"] + BOARD_BYTES] = b"\xa5" * BOARD_BYTES
            flags = activity(cells, row_activity)
            flag_address = sym["active_rows" if row_activity else "active_tiles"]
            image[flag_address:flag_address + len(flags)] = flags
            guard = sym["__bss_end"]
            image[guard:guard + 16] = b"\x5a" * 16
            records, memory = self.simulate(directory, image, 0x2000,
                                           [(0xa000, 0x5800)], points)
            check(records[-1]["halt"] == "idle", f"{case} frame run did not halt")
            registers = [int(value, 16) for value in records[-1]["r"]]
            check(registers[10] == 0xfffc and registers[15] == 0, "frame ABI/counter mismatch")
            events = records[:-1]
            check(len(events) == 4 * frames, f"{case} tracepoint count mismatch")
            step_cycles, draw_cycles = [], []
            for index in range(0, len(events), 4):
                a, b, c, d = events[index:index + 4]
                check([int(e["pc"], 16) for e in [a, b, c, d]] == points, "frame trace order")
                step_cycles.append(b["clk"] - a["clk"])
                draw_cycles.append(d["clk"] - c["clk"])
            board_address = sym["board_a" if frames % 2 == 0 else "board_b"]
            check(memory[board_address:board_address + BOARD_BYTES] == pack(expected),
                  f"{case} -O{opt}: final board mismatch")
            address = sym["dirty_tiles"]
            check(memory[address:address + TILE_ROWS * TILE_COLS] == dirty_map(previous, expected),
                  f"{case} -O{opt}: dirty map mismatch")
            check(memory[flag_address:flag_address + len(flags)] == activity(expected, row_activity),
                  f"{case} -O{opt}: next activity mismatch")
            check(memory[guard:guard + 16] == image[guard:guard + 16], "frame guard overwrite")
            total = [step + draw for step, draw in zip(step_cycles, draw_cycles)]
            result = dict(label=label, opt=opt, case=case, frames=frames,
                          layout="cart" if "cart_header" in sym else "EA5",
                          image_bytes=len(rom), rom=len(rom) if "cart_header" in sym else None,
                          ram=sym["__bss_end"] - sym["__data_start"],
                          step_mean=statistics.mean(step_cycles), step_sd=statistics.pstdev(step_cycles),
                          draw_mean=statistics.mean(draw_cycles), draw_sd=statistics.pstdev(draw_cycles),
                          sum_mean=statistics.mean(total), sum_sd=statistics.pstdev(total))
            results.append(result)
            print(json.dumps(result), flush=True)
        return results

    def render_tests(self, cart, opt):
        directory, sym, rom = cart
        writes = self.vdp_writes(directory)
        _, code = self.probe(directory, f'''.text
.globl _start
_start:
  LWPI 0x8300
  LI R10,0xfffc
  LI R13,0xd013
  LI R14,0xd014
  LI R15,0xd015
  LI R0,{sym['board_b']}
  BL @{sym['vdp_write_dirty_patterns']}
  IDLE
''')
        board = bytes(range(256)) * 6
        # Dirty only some rows, including first/last, and only one tile per row.
        # Streaming must redraw the complete dirty row and skip every clean row.
        for rows in [set(range(TILE_ROWS)), {0, 7, 8, 15, 16, 23}, set()]:
            image = self.cart_memory(sym, rom)
            image[0x2000:0x2000 + len(code)] = code
            image[sym["board_b"]:sym["board_b"] + BOARD_BYTES] = board
            flags = bytearray(TILE_ROWS * TILE_COLS)
            for ty in rows:
                flags[ty * TILE_COLS + (ty % TILE_COLS)] = 1
            image[sym["dirty_tiles"]:sym["dirty_tiles"] + len(flags)] = flags
            records, _ = self.simulate(directory, image, 0x2000, points=writes)
            check(records[-1]["halt"] == "idle", "render did not halt")
            registers = [int(value, 16) for value in records[-1]["r"]]
            check(registers[10] == 0xfffc and registers[13:16] == [0xd013, 0xd014, 0xd015],
                  "render ABI mismatch")
            actual = [(writes[int(event["pc"], 16)][1],
                       int(event["r"][writes[int(event["pc"], 16)][0]], 16) >> 8)
                      for event in records[:-1]]
            expected = []
            for ty in sorted(rows):
                expected += [(0x8c02, 0), (0x8c02, 0x40 + ty)]
                for tx in range(TILE_COLS):
                    for row in range(4):
                        value = 0
                        for cx in range(4):
                            x, y = tx * 4 + cx, ty * 4 + row
                            if board[y * ROW_BYTES + x // 8] & (0x80 >> (x % 8)):
                                value |= 3 << (6 - 2 * cx)
                        expected += [(0x8c00, value)] * 2
            check(actual == expected, f"-O{opt}: VDP byte/address stream mismatch")
        print(f"render -O{opt}: all 256 byte values, full/partial/clean rows and ABI PASS", flush=True)

    def snapshot_tests(self, opt):
        for name in ["life2_2x", "life3_2x"]:
            directory, sym, rom = self.build_cart(opt, name, snapshot=True)
            for hits, expected in [(2, 1), (3, 0)]:
                image = self.program_memory(sym, rom)
                records, memory = self.simulate(directory, image, sym["_start"],
                                               [(0xa000, 0x5800)],
                                               [sym["vdp_write_dirty_patterns"]],
                                               ["--tracepoint-max", hits])
                check(records[-1]["halt"] == "tracepoint-max", "snapshot run did not stop")
                check(memory[sym["debug_snapshot_pending"]] == expected,
                      f"{name} -O{opt}: snapshot cleared in wrong generation")
                if hits == 3:
                    start = sym["dirty_prev"]
                    check(memory[start:start + TILE_ROWS * TILE_COLS] == bytes(TILE_ROWS * TILE_COLS),
                          "snapshot clear did not reset color tracking")
            print(f"{name} -O{opt}: F snapshot survives its draw and clears next generation PASS", flush=True)

    def perturb_tests(self, cart, opt):
        directory, sym, rom = cart
        # Cover every disk radius, each signed wrapping direction, and corners.
        seeds = [1, 0x1d2b, 0xffff, 0xabcd]
        blob_seeds = set(seeds)
        radii, placements = set(), set()
        for state in range(1, 65536):
            geometry = []
            interactive_addition(set(), state, "M", geometry)
            x0, y0, radius = geometry[0]
            edges = set()
            if x0 - radius < 0:
                edges.add("left")
            if x0 + radius >= WIDTH:
                edges.add("right")
            if y0 - radius < 0:
                edges.add("top")
            if y0 + radius >= HEIGHT:
                edges.add("bottom")
            if edges & {"left", "right"} and edges & {"top", "bottom"}:
                edges.add("corner")
            if not edges:
                edges.add("interior")
            if radius not in radii or edges - placements:
                blob_seeds.add(state)
                radii.add(radius)
                placements.update(edges)
            if len(radii) == 12 and len(placements) == 6:
                break
        check(radii == set(range(12, 24)) and len(placements) == 6,
              "could not construct disk radius/wrapping fixtures")
        boards = [set(), {(0, 0), (95, 127), (40, 64), (41, 64), (42, 64)},
                  {(y, x) for y in range(HEIGHT) for x in range(WIDTH)}]
        for key, function in [("N", "add_noise"), ("M", "add_blob")]:
            _, code = self.probe(directory, f'''.text
.globl _start
_start:
 LWPI 0x8300
 LI R10,0xfffc
 LI R13,0xd013
 LI R14,0xd014
 LI R15,0xd015
 LI R0,0x9000
 BL @{sym[function]}
 IDLE
''')
            for seed in seeds if key == "N" else sorted(blob_seeds):
                for cells in boards:
                    expected, final_rng = interactive_addition(cells, seed, key)
                    image = self.cart_memory(sym, rom)
                    image[0x2000:0x2000 + len(code)] = code
                    image[0x9000:0x9600] = pack(cells)
                    image[0x8fc0:0x9000] = b"\xa5" * 64
                    image[0x9600:0x9640] = b"\x5a" * 64
                    address = sym["rng_state"]
                    image[address:address + 2] = seed.to_bytes(2, "big")
                    flags = bytearray(TILE_ROWS * TILE_COLS)
                    flags[5 * TILE_COLS + 7] = 1 # Existing dirt must be preserved.
                    address = sym["dirty_tiles"]
                    image[address:address + len(flags)] = flags
                    address = sym["active_rows"]
                    image[address:address + TILE_ROWS] = activity(cells, True)
                    records, memory = self.simulate(directory, image, 0x2000,
                                                   [(0x8fc0, 0x680), (0xa000, 0x5800)])
                    check(records[-1]["halt"] == "idle", f"{key} helper did not return")
                    registers = [int(value, 16) for value in records[-1]["r"]]
                    check(registers[10] == 0xfffc and registers[13:16] == [0xd013, 0xd014, 0xd015],
                          f"{key} helper ABI mismatch")
                    check(memory[0x9000:0x9600] == pack(expected), f"{key} cell/RNG sequence mismatch")
                    check(memory[0x8fc0:0x9000] == image[0x8fc0:0x9000] and
                          memory[0x9600:0x9640] == image[0x9600:0x9640], f"{key} board overrun")
                    changed = dirty_map(cells, expected)
                    address = sym["dirty_tiles"]
                    check(memory[address:address + len(flags)] == bytes(a | b for a, b in zip(flags, changed)),
                          f"{key} dirty flags mismatch")
                    address = sym["active_rows"]
                    check(memory[address:address + TILE_ROWS] == activity(expected, True), f"{key} activity mismatch")
                    address = sym["rng_state"]
                    check(int.from_bytes(memory[address:address + 2], "big") == final_rng, f"{key} final RNG mismatch")
            # Exercise the real key dispatch, repeat suppression, uppercase,
            # release and lowercase. Halt during scan 6, after the fifth step.
            image = self.program_memory(sym, rom)
            self.keyboard_stub(directory, image, sym, (32, ord(key), ord(key), 0, ord(key.lower()), None))
            expected, state = initial_seed((self.demos / "life3_2x.c").read_text())
            for index in range(1, 6):
                expected = generation(expected)
                if index in (1, 4):
                    expected, state = interactive_addition(expected, state, key)
            records, memory = self.simulate(directory, image, sym["_start"], [(0xa000, 0x5800)], [sym[function]])
            check(records[-1]["halt"] == "idle" and len(records[:-1]) == 2,
                  f"{key} key handling repeated while held or missed lowercase")
            address = sym["board_b"]
            check(memory[address:address + BOARD_BYTES] == pack(expected), f"{key} real main board mismatch")
            address = sym["rng_state"]
            check(int.from_bytes(memory[address:address + 2], "big") == state, f"{key} main RNG mismatch")
        print(f"N/M -O{opt}: exact additions, radii 12-23, all wrapped edges, guards, dirty/activity, ABI and held/released keys PASS", flush=True)

    def startup_tests(self, opt, life3=None):
        for name in ["life2x", "life2_2x", "life3_2x"]:
            cart = life3 if name == "life3_2x" and life3 else self.build_cart(opt, name)
            directory, sym, rom = cart
            image = self.program_memory(sym, rom)
            if "life_splash_ready" in sym:
                self.keyboard_stub(directory, image, sym)
            guard = sym["__bss_end"]
            image[guard:guard + 16] = b"\xa5" * 16
            records, memory = self.simulate(directory, image, sym["_start"], [(0xa000, 0x5800)],
                                           extra=["-s", hex(sym["step_board"])])
            check(records[-1]["halt"] == "stop", f"{name} startup did not reach first step")
            expected, rng_state = initial_seed((self.demos / (name + ".c")).read_text())
            address = sym["board_a"]
            check(memory[address:address + BOARD_BYTES] == pack(expected),
                  f"{name} -O{opt}: startup seed differs from scalar reference")
            address = sym["board_b"]
            check(memory[address:address + BOARD_BYTES] == bytes(BOARD_BYTES),
                  f"{name} seed overwrote the other board")
            row_activity = "active_rows" in sym
            flags = activity(expected, row_activity)
            address = sym["active_rows" if row_activity else "active_tiles"]
            check(memory[address:address + len(flags)] == flags, "startup activity mismatch")
            address = sym["rng_state"]
            check(int.from_bytes(memory[address:address + 2], "big") == rng_state,
                  "startup RNG state mismatch")
            check(memory[guard:guard + 16] == image[guard:guard + 16], "startup BSS guard overwritten")
            print(f"{name} -O{opt}: actual CRT/main initial seed, RNG, activity and guards PASS", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--llvm-bin", type=Path, default=ROOT / "llvm-project/build/bin")
    parser.add_argument("--trace", type=Path, default=ROOT.parent / "tms9900-trace/build/tms9900-trace")
    parser.add_argument("--demo-dir", type=Path, default=ROOT / "cart_example")
    parser.add_argument("--opt", nargs="+", choices=["0", "1", "2", "3", "s", "z"],
                        default=["0", "1", "2", "3", "s", "z"])
    parser.add_argument("--filter", nargs="+", choices=["seed", "row", "frames", "render", "snapshot", "geometry", "startup", "splash", "perturb"],
                        default=["seed", "row", "frames", "render", "snapshot", "geometry", "startup", "splash", "perturb"])
    parser.add_argument("--frames", type=int, default=64)
    parser.add_argument("--baseline-dir", type=Path,
                        help="Compare with saved life3_2x.elf/.bin and use that cart's initial board")
    parser.add_argument("--baseline-opt", choices=["0", "1", "2", "3", "s", "z"], default="3",
                        help="Optimization level of the saved image, for result labels (default: 3)")
    parser.add_argument("--results", type=Path, help="Save raw cycle statistics as JSON")
    parser.add_argument("--splash-preview", type=Path, help="Render captured splash VRAM to a PNG")
    args = parser.parse_args()
    if args.frames < 1:
        parser.error("--frames must be positive")
    with tempfile.TemporaryDirectory(prefix="tms9900-life-tests-") as directory:
        runner = Runner(args.llvm_bin.resolve(), args.trace.resolve(), args.demo_dir.resolve(), Path(directory))
        randomizer = random.Random(9900)
        cases = {
            "sparse-wrap": {(95, 127), (95, 0), (95, 1), (0, 1), (1, 0),
                            (40, 63), (40, 64), (40, 65), (60, 0), (61, 0), (62, 0)},
            "random": {(y, x) for y in range(HEIGHT) for x in range(WIDTH)
                       if randomizer.random() < .35},
            "empty": set(),
        }
        results = []
        if args.baseline_dir:
            baseline = args.baseline_dir.resolve()
            sym = runner.symbols(baseline / "life3_2x.elf")
            rom = (baseline / "life3_2x.bin").read_bytes()
            image = runner.program_memory(sym, rom)
            if "life_splash_ready" in sym:
                runner.keyboard_stub(runner.directory, image, sym)
            records, memory = runner.simulate(runner.directory, image, sym["_start"],
                                             [(sym["board_a"], BOARD_BYTES)],
                                             extra=["-s", hex(sym["step_board"])])
            check(records[-1]["halt"] == "stop", "baseline startup did not reach first step")
            cases = {"demo-seed": unpack(memory[sym["board_a"]:sym["board_a"] + BOARD_BYTES]), **cases}
            if "frames" in args.filter:
                comparison = runner.directory / "baseline"
                comparison.mkdir()
                results += runner.frame_tests((comparison, sym, rom), args.baseline_opt, cases, args.frames, "baseline")
        if "row" in args.filter:
            runner.row_tests()
        if "geometry" in args.filter:
            source = (runner.demos / "life3_2x.c").read_text()
            for old, new in [("#define CELL_W SCREEN_CELL_W", "#define CELL_W 96"),
                             ("#define CELL_H SCREEN_CELL_H", "#define CELL_H 80"),
                             ("#define SCALE 2", "#define SCALE 1")]:
                check(old in source, "geometry test no longer matches source definitions")
                path = runner.directory / "geometry.c"
                path.write_text(source.replace(old, new, 1))
                result = subprocess.run([str(runner.tools / "clang"), "--target=tms9900",
                                         "-fsyntax-only", "-ffreestanding", "-I", str(runner.demos), str(path)],
                                        text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                check(result.returncode != 0 and "Life3 requires" in result.stderr,
                      "unsupported assembly/render geometry was accepted")
            print("geometry: unsupported width/height/scale rejected PASS", flush=True)
        for opt in args.opt:
            cart = None
            if "seed" in args.filter:
                runner.seed_tests(opt)
            if any(name in args.filter for name in ["frames", "render", "splash", "perturb"]):
                cart = runner.build_cart(opt)
                if "frames" in args.filter:
                    results += runner.frame_tests(cart, opt, cases, args.frames)
                if "render" in args.filter:
                    runner.render_tests(cart, opt)
                if "splash" in args.filter:
                    runner.splash_tests(cart, opt, args.splash_preview)
                if "perturb" in args.filter:
                    runner.perturb_tests(cart, opt)
            if "snapshot" in args.filter:
                runner.snapshot_tests(opt)
            if "startup" in args.filter:
                runner.startup_tests(opt, cart)
        if args.results:
            args.results.write_text(json.dumps(results, indent=2) + "\n")
    print("All requested Life checks passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
