from pathlib import Path
import re
import sys


root = Path(sys.argv[1])
violations: list[str] = []
for path in sorted((root / "src" / "core").rglob("*.cpp")):
    text = path.read_text(encoding="utf-8")
    relative = path.relative_to(root).as_posix()
    if relative != "src/core/random/random_hub.cpp" and (
        "pcg_random.hpp" in text or re.search(r"\bpcg64\b", text)
    ):
        violations.append(f"{relative}: direct PCG access outside RandomHub")
    for pattern, label in (
        (r"#\s*include\s*<random>", "standard random header"),
        (r"\bstd::(?:uniform_\w+_distribution|shuffle|random_device|mt19937)\b",
         "forbidden standard random API"),
    ):
        if re.search(pattern, text):
            violations.append(f"{relative}: {label}")

if violations:
    raise SystemExit("\n".join(violations))
