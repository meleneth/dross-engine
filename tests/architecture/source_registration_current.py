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
    if unregistered:
        print("project implementation files missing from CMake:")
        print("\n".join(unregistered))
    if missing:
        print("CMake source entries missing from the source tree:")
        print("\n".join(missing))
    return 1 if unregistered or missing else 0


if __name__ == "__main__":
    raise SystemExit(main())
