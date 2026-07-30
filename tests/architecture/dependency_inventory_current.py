from __future__ import annotations

from pathlib import Path
import re
import sys


ACTIVE_DEPENDENCIES = {
    "Catch2": ("Catch2", True),
    "EnTT": ("EnTT", False),
    "sml": ("Boost.Ext.SML", False),
    "eventpp": ("eventpp", False),
    "pcg_cpp": ("pcg-cpp", False),
    "tl_expected": ("tl::expected", False),
    "blake3": ("BLAKE3", False),
    "godot_cpp": ("godot-cpp", False),
}

CPM_PACKAGE = re.compile(
    r"CPMAddPackage\(\s*NAME\s+([A-Za-z0-9_]+).*?\bGIT_TAG\s+([^\s\)]+)",
    re.DOTALL,
)


def table_rows(text: str) -> dict[str, str]:
    rows: dict[str, str] = {}
    for line in text.splitlines():
        match = re.match(r"\|\s*([^|]+?)\s*\|\s*([^|]+?)\s*\|", line)
        if match:
            rows[match.group(1)] = match.group(2)
    return rows


def main() -> int:
    root = Path(sys.argv[1])
    dependencies = (root / "cmake" / "Dependencies.cmake").read_text(encoding="utf-8")
    lock_rows = table_rows((root / "docs" / "dependency-lock.md").read_text(encoding="utf-8"))
    notice_rows = table_rows((root / "THIRD_PARTY_NOTICES.md").read_text(encoding="utf-8"))
    configured = dict(CPM_PACKAGE.findall(dependencies))
    failures: list[str] = []

    if configured.keys() != ACTIVE_DEPENDENCIES.keys():
        missing = sorted(ACTIVE_DEPENDENCIES.keys() - configured.keys())
        unexpected = sorted(configured.keys() - ACTIVE_DEPENDENCIES.keys())
        if missing:
            failures.append(f"expected active dependencies are not configured: {', '.join(missing)}")
        if unexpected:
            failures.append(
                "configured dependencies need inventory classification: "
                + ", ".join(unexpected)
            )

    for package, pin in sorted(configured.items()):
        if package not in ACTIVE_DEPENDENCIES:
            continue
        display_name, build_only = ACTIVE_DEPENDENCIES[package]
        locked_pin = lock_rows.get(display_name)
        if locked_pin is None:
            failures.append(f"{display_name} is missing from docs/dependency-lock.md")
        elif pin not in locked_pin:
            failures.append(
                f"{display_name} pin differs: CMake has {pin}, lock has {locked_pin}"
            )
        if not build_only and display_name not in notice_rows:
            failures.append(f"{display_name} is missing from THIRD_PARTY_NOTICES.md")

    if failures:
        print("\n".join(failures))
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
