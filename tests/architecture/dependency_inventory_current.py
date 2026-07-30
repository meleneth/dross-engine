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
    license_text = (root / "LICENSE").read_text(encoding="utf-8")
    readme = (root / "README.md").read_text(encoding="utf-8")
    normalized_readme = " ".join(readme.split())
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

    for required_license_text in (
        "MIT License",
        "Copyright (c) 2026 Dross Engine contributors",
        "Permission is hereby granted, free of charge",
        'THE SOFTWARE IS PROVIDED "AS IS"',
    ):
        if required_license_text not in license_text:
            failures.append(f"LICENSE is missing MIT term: {required_license_text}")

    for required_readme_text in (
        "Dross Engine is available under the permissive [MIT License](LICENSE).",
        "Games and applications built with it may use their own license, including proprietary",
    ):
        if required_readme_text not in normalized_readme:
            failures.append(f"README license policy drifted: {required_readme_text}")

    for package_script in ("package-linux", "package-windows"):
        script = (root / "bin" / package_script).read_text(encoding="utf-8")
        for required_file in ("LICENSE", "THIRD_PARTY_NOTICES.md", "SHA256SUMS"):
            if required_file not in script:
                failures.append(f"{package_script} does not distribute {required_file}")

    if failures:
        print("\n".join(failures))
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
