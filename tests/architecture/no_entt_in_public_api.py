#!/usr/bin/env python3

from pathlib import Path
import sys


def main() -> int:
    root = Path(sys.argv[1])
    violations: list[str] = []
    for header in sorted((root / "include").rglob("*.hpp")):
        text = header.read_text(encoding="utf-8")
        if "entt::entity" in text or "#include <entt/" in text:
            violations.append(str(header.relative_to(root)))

    if violations:
        print("EnTT storage details leaked into public headers:")
        print("\n".join(violations))
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
