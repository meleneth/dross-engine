from __future__ import annotations

from pathlib import Path
import re
import sys


SOURCE_PATH = re.compile(r"^\s*([A-Za-z0-9_./-]+\.(?:c|cc|cpp))\s*$", re.MULTILINE)
SOURCE_ROOTS = (
    "src",
    "tools",
    "tests/unit",
    "generated/src",
    "generated/tests",
)
PRIVATE_HEADER_ROOTS = (
    "src/godot",
    "tools/dross_cli",
)
INCLUDE = re.compile(r"^\s*#\s*include\s*[<\"]([^>\"]+)[>\"]", re.MULTILINE)


def main() -> int:
    root = Path(sys.argv[1])
    cmake_text = (root / "CMakeLists.txt").read_text(encoding="utf-8")
    registered = set(SOURCE_PATH.findall(cmake_text))
    present = {
        path.relative_to(root).as_posix()
        for source_root in SOURCE_ROOTS
        for path in (root / source_root).rglob("*")
        if path.suffix in {".c", ".cc", ".cpp"}
    }

    unregistered = sorted(present - registered)
    missing = sorted(registered - present)
    included_names = {
        Path(include).name
        for source_root in SOURCE_ROOTS
        for path in (root / source_root).rglob("*")
        if path.suffix in {".h", ".hh", ".hpp", ".c", ".cc", ".cpp"}
        for include in INCLUDE.findall(path.read_text(encoding="utf-8"))
    }
    orphan_private_headers = sorted(
        path.relative_to(root).as_posix()
        for header_root in PRIVATE_HEADER_ROOTS
        for path in (root / header_root).rglob("*")
        if path.suffix in {".h", ".hh", ".hpp"} and path.name not in included_names
    )
    if unregistered:
        print("project implementation files missing from CMake:")
        print("\n".join(unregistered))
    if missing:
        print("CMake source entries missing from the source tree:")
        print("\n".join(missing))
    if orphan_private_headers:
        print("private headers with no include consumer:")
        print("\n".join(orphan_private_headers))
    return 1 if unregistered or missing or orphan_private_headers else 0


if __name__ == "__main__":
    raise SystemExit(main())
