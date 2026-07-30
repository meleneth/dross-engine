# Phase 14 Windows Package Preflight

Date: 2026-07-29

## Result

The Windows packaging workflow is present but not platform-validated.

The platform-independent portions pass:

- `bin/package-windows` passes ShellCheck;
- the non-Windows host guard rejects execution before configuring a build;
- Godot 4.7.1 recognizes the `Windows Prototype` export preset;
- that preset exports a non-empty runtime PCK with the intended production
  resource filter on Linux.

The script is designed to run from Git Bash on Windows. It builds the
`windows-msvc-release` preset, exports the runtime pack, stages the Godot
executable and `dross_godot.dll`, smoke-launches the package, rejects Godot
runtime errors, installs licensing files, and creates a checksum manifest.

## Validated commands

```sh
shellcheck bin/package-linux bin/package-windows

env -u OS GODOT_BIN=/not/a/godot \
  bin/package-windows /tmp/dross-windows-negative

godot --headless --path godot \
  --export-pack "Windows Prototype" \
  /tmp/dross-phase14-windows-resources.pck
```

## Unverified boundary

No Windows host or runner is available. The MSVC build, Windows GDExtension
load, packaged executable smoke launch, and resulting artifact hashes therefore
remain unverified. The existence and Linux preflight of this workflow are not a
Windows compatibility claim.
