#!/usr/bin/env python3
"""Track build settings and invalidate generated files when they change.

--check is read-only, including during make -n. --update removes only matching
files in the stamp's own directory. It never recurses or removes source trees.
"""

import argparse
import json
from pathlib import Path
import shlex
import shutil
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("stamp", type=Path)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--check", action="store_true")
    mode.add_argument("--update", action="store_true")
    parser.add_argument("--value", action="append", default=[])
    parser.add_argument("--tool", action="append", default=[])
    parser.add_argument("--file", action="append", default=[], type=Path)
    parser.add_argument("--remove", action="append", default=[])
    args = parser.parse_args()
    settings = {"values": args.value, "tools": {}, "files": {}}
    for command in args.tool:
        words = shlex.split(command)
        executable = words[0] if words else ""
        location = shutil.which(executable)
        path = Path(location or executable).resolve()
        try:
            stat = path.stat()
            fingerprint = [str(path), stat.st_mtime_ns, stat.st_size]
        except OSError:
            fingerprint = [str(path), "missing"]
        settings["tools"][command] = fingerprint
    for path in args.file:
        path = path.resolve()
        try:
            stat = path.stat()
            fingerprint = [stat.st_mtime_ns, stat.st_size]
        except OSError:
            fingerprint = ["missing"]
        settings["files"][str(path)] = fingerprint
    try:
        previous = json.loads(args.stamp.read_text())
    except (OSError, ValueError):
        previous = None
    changed = previous != settings
    if args.check:
        print("changed" if changed else "unchanged")
        return 0
    if not changed:
        return 0
    parent = args.stamp.parent
    # Validate the complete request before deleting any generated outputs.
    for pattern in args.remove:
        if "/" in pattern or "\\" in pattern:
            parser.error("invalidation patterns must stay in the stamp directory")
    for pattern in args.remove:
        for path in parent.glob(pattern):
            if path != args.stamp and (path.is_file() or path.is_symlink()):
                path.unlink()
    parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(mode="w", dir=parent, delete=False) as output:
        json.dump(settings, output, indent=2)
        output.write("\n")
        temporary = Path(output.name)
    temporary.replace(args.stamp)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
