#!/usr/bin/env python3
"""Check partial rebuilds, flags, headers, tool changes, and read-only dry runs."""

import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parents[2]


def run(command, cwd=None):
    result = subprocess.run([str(arg) for arg in command], cwd=cwd, text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if result.returncode:
        raise RuntimeError(f"{command}\n{result.stdout}{result.stderr}")
    return result.stdout


def snapshot(directory):
    return {path.name: (path.read_bytes(), path.stat().st_mtime_ns)
            for path in directory.iterdir() if path.is_file()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--llvm-bin", type=Path, default=ROOT / "llvm-project/build/bin")
    parser.add_argument("--host-only", action="store_true")
    args = parser.parse_args()
    tools = args.llvm_bin.resolve()
    cc = shutil.which("cc")
    if not cc:
        parser.error("a host C compiler is needed for the Makefile fixture")
    count = 0

    def check(condition, name):
        nonlocal count
        if not condition:
            raise RuntimeError(f"FAIL: {name}")
        count += 1
        print(f"PASS: {name}", flush=True)

    with tempfile.TemporaryDirectory(prefix="tms9900-build-tests-") as directory:
        work = Path(directory)
        fixture = work / "fixture"
        fixture.mkdir()
        helper = ROOT / "tools/build_config.mk"
        (fixture / "Makefile").write_text(f""".DEFAULT_GOAL := all
CC = {cc}
CFLAGS = -O0 -DVALUE=1
TAG = initial
all: app
app: a.o b.o
\t$(CC) $(filter %.o,$^) -o $@
%.o: %.c
\t$(CC) $(CFLAGS) -MMD -MP -c $< -o $@
CONFIG_STAMP = .build-config.json
CONFIG_ARGS = --value=$(call quote_config,CFLAGS=$(CFLAGS)) \\
              --value=$(call quote_config,TAG=$(TAG)) --tool=$(call quote_config,$(CC))
CONFIG_OUTPUTS = a.o b.o app
CONFIG_PATTERNS = *.o *.d app
include {helper}
-include $(wildcard *.d)
""")
        (fixture / "value.h").write_text("#define EXTRA 3\n")
        (fixture / "a.c").write_text('#include "value.h"\n#include <stdio.h>\n'
                                     'int b(void); int main(void) { printf("%d\\n", VALUE+EXTRA+b()); }\n')
        (fixture / "b.c").write_text('#include "value.h"\nint b(void) { return VALUE+EXTRA; }\n')
        (fixture / "notes.md").write_text("Keep local source/plans.\n")
        (fixture / "nested").mkdir()
        (fixture / "nested/keep.o").write_bytes(b"not in the build directory")
        run(["make", "-j4"], fixture)
        check(run([fixture / "app"]).strip() == "8", "initial parallel build")
        before = snapshot(fixture)
        run(["make", "-j4"], fixture)
        check(snapshot(fixture) == before, "same configuration preserves all artifacts")

        changed = ["CFLAGS=-O2 -DVALUE=9", "TAG=two words and an apostrophe: ' preserved"]
        run(["make", "-n", "a.o", *changed], fixture)
        check(snapshot(fixture) == before, "make -n with new flags is read-only")
        run(["make", "a.o", *changed], fixture)
        check(not (fixture / "b.o").exists() and not (fixture / "app").exists(),
              "partial flag change invalidates unrequested outputs")
        config = json.loads((fixture / ".build-config.json").read_text())
        check(config["values"] == ["CFLAGS=-O2 -DVALUE=9", changed[1]],
              "spaces and apostrophes survive configuration recording")
        check((fixture / "notes.md").exists() and (fixture / "nested/keep.o").exists(),
              "invalidation leaves sources and nested files alone")
        run(["make", "-j4", *changed], fixture)
        check(run([fixture / "app"]).strip() == "24", "later partial build uses new flags throughout")

        # macOS system make compares coarse timestamps; cross a second for this
        # header-only edit. Flag invalidation above deliberately needs no delay.
        time.sleep(1.05)
        (fixture / "value.h").write_text("#define EXTRA 5\n")
        run(["make", "-j4", *changed], fixture)
        check(run([fixture / "app"]).strip() == "28", "header dependencies rebuild all consumers")

        wrapper = fixture / "compiler"
        wrapper.write_text(f'#!/bin/sh\nexec "{cc}" "$@"\n')
        wrapper.chmod(0o755)
        tool_args = [*changed, f"CC={wrapper}"]
        run(["make", "a.o", *tool_args], fixture)
        check(not (fixture / "b.o").exists(), "compiler path change invalidates outputs")
        run(["make", "-j4", *tool_args], fixture)
        stat = wrapper.stat()
        os.utime(wrapper, ns=(stat.st_atime_ns, stat.st_mtime_ns + 1000000000))
        run(["make", "a.o", *tool_args], fixture)
        check(not (fixture / "b.o").exists(), "compiler replacement at the same path invalidates outputs")

        if not args.host_only:
            for library, member in [("builtins", "malloc.o"), ("libm", "sf_sin.o")]:
                source = ROOT / "libtms9900" / library
                archive = "libbuiltins.a" if library == "builtins" else "libm.a"
                flags = "--target=tms9900 -ffreestanding -fno-builtin"
                if library == "builtins":
                    flags += " -c"
                else:
                    flags += (" -I ../picolibc/libc/include -I ../picolibc/libm/common"
                              " -I include -include include/picolibc.h -D_COMPILING_NEWLIB")
                build, fresh = work / library, work / (library + "-fresh")
                base = ["make", "-j4", f"CC={tools / 'clang'}", f"AR={tools / 'llvm-ar'}"]
                run([*base, f"BUILD={build}", f"CFLAGS={flags} -O0", build / archive], source)
                run([*base, f"BUILD={build}", f"CFLAGS={flags} -O2", build / member], source)
                check(not (build / archive).exists(), f"{library} partial optimization switch invalidates archive")
                run([*base, f"BUILD={build}", f"CFLAGS={flags} -O2", build / archive], source)
                run([*base, f"BUILD={fresh}", f"CFLAGS={flags} -O2", fresh / archive], source)
                check((build / archive).read_bytes() == (fresh / archive).read_bytes(),
                      f"{library} incremental archive equals a fresh build")
                check(".build-config.json" not in run([tools / "llvm-ar", "t", build / archive]),
                      f"{library} archive contains only object members")
                before = snapshot(build)
                run([*base, f"BUILD={build}", f"CFLAGS={flags} -O2", build / archive], source)
                check(snapshot(build) == before, f"{library} repeated build preserves artifacts")

            demos = work / "cart_example"
            demos.mkdir()
            (work / "tools").symlink_to(ROOT / "tools", target_is_directory=True)
            for name in ["Makefile", "ball.c", "banner.c", "crt0.s", "cart.ld", "ea5.ld",
                         "life3_2x.c", "life3_splash.h", "life3_2x_step.S", "kbd.S"]:
                shutil.copy2(ROOT / "cart_example" / name, demos / name)
            for name in ["bin2ea5.py", "bin2rpk.py"]:
                shutil.copy2(ROOT / name, work / name)
            base = ["make", "-j4", f"LLVM_BUILD={tools}"]
            run([*base, "ball.bin", "banner.bin"], demos)
            flags = "--target=tms9900 -O0 -ffreestanding -fno-builtin -Wno-main-return-type"
            run([*base, "ball.o", f"CFLAGS={flags}"], demos)
            check(not (demos / "banner.o").exists() and not (demos / "banner.bin").exists(),
                  "demo partial flag switch invalidates other programs")
            run([*base, "ball.bin", "banner.bin", f"CFLAGS={flags}"], demos)
            before = snapshot(demos)
            run([*base, "ball.bin", "banner.bin", f"CFLAGS={flags}"], demos)
            check(snapshot(demos) == before, "demo repeated build preserves artifacts")
            run([*base, "ball.elf", f"CFLAGS={flags}", "STACK_SIZE=0x1000"], demos)
            syms = {fields[2]: int(fields[0], 16)
                    for line in run([tools / "llvm-nm", demos / "ball.elf"]).splitlines()
                    if len(fields := line.split()) == 3 and fields[1].lower() != "u"}
            check(syms["__heap_end"] == 0xf000 and syms["__stack_top"] == 0xfffc,
                  "demo stack reservation switch reaches the linker")
            check(not (demos / "banner.bin").exists(),
                  "demo link configuration switch invalidates unrequested images")

            # Check the effective optimization through actual emitted objects,
            # including command-line precedence and partial configuration changes.
            def life_object(opt):
                reference = work / "reference-life.o"
                run([tools / "clang", "--target=tms9900", f"-O{opt}", "-fno-builtin",
                     "-ffreestanding", "-Wno-main-return-type", "-c", "life3_2x.c",
                     "-o", reference], demos)
                return reference.read_bytes()

            run([*base, "ball.bin", "banner.bin", "life3_2x.o"], demos)
            check((demos / "life3_2x.o").read_bytes() == life_object("2"),
                  "Life3 defaults to O2 while other demo defaults remain O3")
            before = snapshot(demos)
            run([*base, "life3_2x.o"], demos)
            check(snapshot(demos) == before, "Life3 default flags do not cause perpetual rebuilding")
            run([*base, "life3_2x.o", "LIFE3_OPT=-O3"], demos)
            check((demos / "life3_2x.o").read_bytes() == life_object("3"),
                  "Life3 optimization override reaches code generation")
            check(not (demos / "banner.bin").exists(),
                  "Life3 flag change invalidates unrequested images")
            run([*base, "life3_2x.o", "LIFE3_OPT=-O3", f"CFLAGS={flags}"], demos)
            check((demos / "life3_2x.o").read_bytes() == life_object("0"),
                  "explicit CFLAGS override Life3 default and tuning flags")
            makefile = demos / "Makefile"
            text = makefile.read_text()
            default = "CFLAGS = --target=tms9900 -O3"
            check(default in text, "demo Makefile optimization fixture matches current default")
            makefile.write_text(text.replace(default, "CFLAGS = --target=tms9900 -O1", 1))
            run([*base, "life3_2x.o", "LIFE3_OPT=-O3"], demos)
            check((demos / "life3_2x.o").read_bytes() == life_object("1"),
                  "editing the global Makefile level also overrides Life3 tuning")
    print(f"{count}/{count} build dependency tests passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
