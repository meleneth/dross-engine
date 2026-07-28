#!/usr/bin/env python3
from pathlib import Path
import re
import sys

root = Path(sys.argv[1])
patterns = re.compile(r"\b(randf|randi|randomize|seed)\s*\(")
violations = []
for path in sorted((root / "godot" / "scripts").glob("*.gd")):
    for line_number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        if patterns.search(line):
            violations.append(f"{path.relative_to(root)}:{line_number}")
if violations:
    raise SystemExit("forbidden authoritative GDScript RNG: " + ", ".join(violations))
