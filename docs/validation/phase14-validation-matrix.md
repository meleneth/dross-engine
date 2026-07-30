# Phase 14 Validation Matrix

Recorded 2026-07-29 through commit `5517810`.

## Results

| Target | Result | Evidence |
| --- | --- | --- |
| Generated APIs | Pass | `generate all` and `check generated` left no diff |
| Generator and schema tests | Pass | 6 of 6 pytest cases |
| Vertical-slice acceptance mapping | Pass with platform exception | 14 of 15 criteria have direct evidence; the combined cross-platform criterion remains open |
| Configured static analysis | Pass | clang-tidy completed all 34 translation units with project warnings treated as errors |
| Architecture audit | Pass | Automated boundary tests and targeted source review found no live violation or dead production abstraction |
| Linux GCC 14 debug | Pass | 183 of 183 CTest cases |
| Linux Clang 19 ASan/UBSan | Pass with environment exception | 183 of 183 CTest cases with LeakSanitizer disabled |
| Godot 4.7.1 Linux release boundary | Pass | 8 of 8 integration scripts |
| Deterministic replay | Pass | 2 checkpoints and 6 events verified |
| Repeated replay recording | Pass | Byte-identical SHA-256 `0e3aad2bb199ad005749e7614aaafae51e1679ca5ff4adcb391808606ef64bb6` |
| Linux x86_64 package | Pass | Fresh release package exported after sanitizer artifact isolation, checksum-verified, and smoke-launched outside the repository |
| Windows | Unverified | No Windows host, runner, or export template is available |
| Steam Deck device | Unverified | No device is available; compatibility is inferred only from the Linux x86_64 package |

The Phase 09 Godot fault-fixture scripts intentionally print two script errors.
The aggregate runner verifies that both faults are contained and then reports
all eight integration scripts passed.

## Commands

```sh
./bin/dross generate all
./bin/dross check generated
PYTHONPATH=tools/dross_cli/src .venv/bin/python -m pytest -q

cmake --build --preset linux-debug
ctest --preset linux-debug --output-on-failure
cmake --build --preset linux-debug --target tidy

cmake --build --preset linux-asan-ubsan
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=print_stacktrace=1 \
  ctest --preset linux-asan-ubsan --output-on-failure

build/linux-debug/dross_headless scenario thump-on-field-mouse \
  --seed 12345 --record /tmp/dross-phase14-final-a.dross-replay
build/linux-debug/dross_headless replay --verify-checkpoints \
  /tmp/dross-phase14-final-a.dross-replay

GODOT_BIN=/path/to/Godot_v4.7.1-stable_linux.x86_64 \
  bin/package-linux /tmp/dross-phase14-package
```

## Sanitizer environment exception

The unmodified sanitizer test command cannot complete Catch2 test discovery in
this execution environment because LeakSanitizer terminates when the process is
supervised through `ptrace`. No test begins and no product defect is reported.
Disabling leak detection allows the same Clang ASan/UBSan binaries to run all
183 tests. LeakSanitizer therefore remains unverified here; ASan and UBSan pass.

This exception does not convert Windows or Steam Deck into tested targets and
does not support a cross-platform release claim.

Sanitizer GDExtension artifacts are emitted under the sanitizer preset build
directory rather than `godot/bin`. A sanitizer rebuild preserved the normal
extension byte-for-byte, and all eight Godot integration scripts then passed
without rebuilding the normal extension. Sanitizer and Godot validation order
is therefore independent.
