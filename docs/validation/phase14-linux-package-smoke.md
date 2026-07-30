# Phase 14 Linux Prototype Package Smoke

Date: 2026-07-29

## Result

The Linux x86_64 prototype package was built outside the source tree and
headless smoke-launched successfully.

The package contains:

- the official Godot 4.7.1 Linux x86_64 executable;
- the exported runtime-only `dross-engine.pck`;
- the release `bin/libdross_godot.so`;
- the Dross Engine `LICENSE`;
- `THIRD_PARTY_NOTICES.md`, including the Godot license notice;
- `SHA256SUMS`;
- the captured smoke log.

The smoke command loads the PCK and native extension from the package directory,
runs the main scene for three frames, and fails if Godot reports an error or
script error.

## Command

```bash
GODOT_BIN=/path/to/Godot_v4.7.1-stable_linux.x86_64 \
  bin/package-linux /tmp/dross-package-output
```

The output directory must be absent or empty. The script configures and builds
the `linux-release` preset before exporting. The checksum manifest covers the
runtime, PCK, extension, and both licensing files.

## Scope

This is a prototype bundle based on the official Godot executable because
export templates are not installed in the current environment. It is not a
size-optimized distribution artifact.

Windows packaging remains unverified because no Windows host or export template
is available. Steam Deck hardware was not available; compatibility is inferred
from the Linux x86_64 package only and is not a device smoke claim.
