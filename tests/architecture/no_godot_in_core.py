from __future__ import annotations

import pathlib
import re
import sys


GODOT_INCLUDE = re.compile(r"^\s*#\s*include\s*[<\"](?:godot_cpp|godot)/", re.MULTILINE)


def main() -> int:
    root = pathlib.Path(sys.argv[1])
    checked_roots = [root / "include" / "dross", root / "src" / "core"]
    violations: list[pathlib.Path] = []

    for checked_root in checked_roots:
        if not checked_root.exists():
            continue
        for path in checked_root.rglob("*"):
            if path.suffix not in {".h", ".hh", ".hpp", ".c", ".cc", ".cpp"}:
                continue
            if GODOT_INCLUDE.search(path.read_text(encoding="utf-8")):
                violations.append(path.relative_to(root))

    if violations:
        for violation in violations:
            print(f"Godot include is forbidden in core: {violation}")
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
