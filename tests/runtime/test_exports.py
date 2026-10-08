#!/usr/bin/env python3
"""Standalone EA5/RPK container checks; no compiler or simulator is needed."""

from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import xml.etree.ElementTree as ET
import zipfile

ROOT = Path(__file__).resolve().parents[2]


class ExportTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="tms9900-exports-")
        self.addCleanup(self.temp.cleanup)
        self.work = Path(self.temp.name)
        self.input = self.work / "demo.bin"
        self.input.write_bytes(b"\x03\x40")

    def invoke(self, tool, *args):
        return subprocess.run([sys.executable, str(ROOT / tool), str(self.input), *args],
                              cwd=self.work, text=True, stdout=subprocess.PIPE,
                              stderr=subprocess.PIPE)

    def test_ea5_default_header(self):
        result = self.invoke("bin2ea5.py")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual((self.work / "demo.img").read_bytes(), b"\0\0\0\x08\xa0\0\x03\x40")

    def test_ea5_24k_option_and_quoted_ti_address(self):
        self.input.write_bytes(bytes(0x5800))
        result = self.invoke("bin2ea5.py", "-a", ">A000", "--max-size", "0x6000")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual((self.work / "demo.img").read_bytes()[:6], b"\0\0\x58\x06\xa0\0")

    def test_ea5_rejects_invalid_ranges_before_writing(self):
        for args in [("-a", "-2"), ("-a", "0x10000"), ("-a", "0xa001"),
                     ("--max-size", "0"), ("--max-size", "0x10000"),
                     ("--max-size", "1"), ("-a", "not-an-address")]:
            with self.subTest(args=args):
                self.assertNotEqual(self.invoke("bin2ea5.py", *args).returncode, 0)
                self.assertFalse((self.work / "demo.img").exists())
        self.input.write_bytes(bytes(3))
        self.assertNotEqual(self.invoke("bin2ea5.py", "-a", "0xfffe").returncode, 0)
        self.assertFalse((self.work / "demo.img").exists())

    def read_rpk(self):
        with zipfile.ZipFile(self.work / "demo.rpk") as archive:
            layout = ET.fromstring(archive.read("layout.xml"))
            metadata = ET.fromstring(archive.read("meta-inf.xml"))
            payload = archive.read(layout.find("resources/rom").get("file"))
            self.assertEqual(layout.find("configuration/pcb/socket").get("uses"), "romimage")
        return layout, metadata, payload

    def test_rpk_name_is_escaped_and_is_not_a_resource_path(self):
        name = "TI's <Life> & /demo/ — cyan"
        result = self.invoke("bin2rpk.py", "--name", name)
        self.assertEqual(result.returncode, 0, result.stderr)
        layout, metadata, payload = self.read_rpk()
        self.assertEqual(metadata.findtext("name"), name)
        self.assertEqual(layout.find("resources/rom").get("file"), "rom.bin")
        self.assertEqual(payload, self.input.read_bytes())

    def test_rpk_uses_header_program_pointer(self):
        data = bytearray(64)
        data[:2] = b"\xaa\x01"
        data[6:8] = b"\x60\x20"
        data[0x24] = 4
        data[0x25:0x29] = b"LIFE"
        self.input.write_bytes(data)
        self.assertEqual(self.invoke("bin2rpk.py").returncode, 0)
        self.assertEqual(self.read_rpk()[1].findtext("name"), "LIFE")

    def test_rpk_pcb_choices(self):
        for size, expected in [(2, "standard"), (8192, "standard"), (16384, "paged378")]:
            with self.subTest(size=size):
                self.input.write_bytes(bytes(size))
                self.assertEqual(self.invoke("bin2rpk.py").returncode, 0)
                self.assertEqual(self.read_rpk()[0].find("configuration/pcb").get("type"), expected)
        self.assertEqual(self.invoke("bin2rpk.py", "--pcb", "standard").returncode, 0)
        self.assertEqual(self.read_rpk()[0].find("configuration/pcb").get("type"), "standard")

    def test_reject_empty_input(self):
        self.input.write_bytes(b"")
        for tool, output in [("bin2ea5.py", "demo.img"), ("bin2rpk.py", "demo.rpk")]:
            with self.subTest(tool=tool):
                self.assertNotEqual(self.invoke(tool).returncode, 0)
                self.assertFalse((self.work / output).exists())


if __name__ == "__main__":
    unittest.main()
