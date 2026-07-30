# Phase 14 Linux Prototype Package Smoke

Date: 2026-07-29
Source baseline: commit `fb69bd5` plus the dependency-audit changes recorded
in this checkpoint.

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

The package was rebuilt after sanitizer GDExtension output was isolated from
the normal Godot output directory. This confirms that a sanitizer build cannot
silently contaminate the subsequent release package.

It was rebuilt again after unused native dependency registrations were removed.
The runtime, PCK, extension, and engine license remained byte-identical; only
the reduced third-party notice hash changed.

## Command

```bash
GODOT_BIN=/path/to/Godot_v4.7.1-stable_linux.x86_64 \
  bin/package-linux /tmp/dross-phase14-package-live-deps

cd /tmp/dross-phase14-package-live-deps
sha256sum -c SHA256SUMS
```

The output directory must be absent or empty. The script configures and builds
the `linux-release` preset before exporting. The checksum manifest covers the
runtime, PCK, extension, and both licensing files.

All five manifest entries passed checksum verification. The captured smoke log
contains only the Godot version banner. The packaged runtime and extension are
stripped Linux x86_64 ELF binaries.

## Artifact hashes

```text
32f8d7596c4b41185512b1c49d69f2da3be018fd784a53e349fa92a98a97bcde  dross-engine.x86_64
784c9ec28a3f75ecc5c1f4c5592d7aa04cbbbc3c305a53ba512046476d3bd7f0  dross-engine.pck
be41136bf9643b0f599d2e3523a8a04e91a498485e7366237c7095dec814f64e  bin/libdross_godot.so
df75416495b97a177c2eacc3e6ccbb3cf80386b08654c728df2960cced8f92a0  LICENSE
be9b5ce0753dc88a4317feaa555694a1de541536b0fc03b34450a042eced20ef  THIRD_PARTY_NOTICES.md
```

## Scope

This is a prototype bundle based on the official Godot executable because
export templates are not installed in the current environment. It is not a
size-optimized distribution artifact.

Windows packaging remains unverified because no Windows host or export template
is available. Steam Deck hardware was not available; compatibility is inferred
from the Linux x86_64 package only and is not a device smoke claim.
