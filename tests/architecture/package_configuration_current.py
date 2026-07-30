from __future__ import annotations

import configparser
from pathlib import Path
import sys


EXPECTED_EXCLUDES = {
    "addons/*",
    "tests/*",
    "thump_demo/scenes/phase10_*",
    "thump_demo/scenes/phase12_*",
    "scripts/phase09_*",
}


def parse_config(path: Path) -> configparser.ConfigParser:
    config = configparser.ConfigParser(interpolation=None)
    config.optionxform = str
    config.read(path, encoding="utf-8")
    return config


def main() -> int:
    root = Path(sys.argv[1])
    exports = parse_config(root / "godot" / "export_presets.cfg")
    extension = parse_config(root / "godot" / "dross.gdextension")
    failures: list[str] = []

    expected_presets = {
        "preset.0": ("Linux Prototype", "Linux"),
        "preset.1": ("Windows Prototype", "Windows Desktop"),
    }
    for section, (name, platform) in expected_presets.items():
        if section not in exports:
            failures.append(f"missing export section [{section}]")
            continue
        preset = exports[section]
        if preset.get("name") != f'"{name}"':
            failures.append(f"{section} has the wrong name")
        if preset.get("platform") != f'"{platform}"':
            failures.append(f"{section} has the wrong platform")
        if preset.get("export_filter") != '"all_resources"':
            failures.append(f"{name} does not export all production resources")
        excludes = {
            item
            for item in preset.get("exclude_filter", '""').strip('"').split(",")
            if item
        }
        if excludes != EXPECTED_EXCLUDES:
            failures.append(f"{name} development-resource exclusions drifted")
        if preset.get("script_export_mode") != "2":
            failures.append(f"{name} does not export compiled scripts")
        if preset.get("encrypt_pck") != "false":
            failures.append(f"{name} unexpectedly encrypts the runtime pack")

    libraries = extension["libraries"] if "libraries" in extension else {}
    expected_libraries = {
        "linux.debug.x86_64": '"res://bin/libdross_godot.so"',
        "linux.release.x86_64": '"res://bin/libdross_godot.so"',
        "windows.debug.x86_64": '"res://bin/dross_godot.dll"',
        "windows.release.x86_64": '"res://bin/dross_godot.dll"',
    }
    for feature, path in expected_libraries.items():
        if libraries.get(feature) != path:
            failures.append(f"GDExtension library mapping drifted for {feature}")

    if failures:
        print("\n".join(failures))
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
