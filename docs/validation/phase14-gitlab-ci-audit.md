# Phase 14 GitLab CI Audit

Date: 2026-07-29

## Result

The GitLab pipeline now encodes the available Phase 14 CI-equivalent commands:

- formatting;
- generated API regeneration, drift detection, and six schema/CLI tests;
- Linux Clang debug build, all native tests, and replay fixture verification;
- Linux GCC release build from an empty CPM cache, all native tests, and replay
  fixture verification;
- project clang-tidy analysis;
- all eight Godot 4.7.1 integration scripts;
- GdUnit4 boundary tests;
- Linux release package build, smoke launch, and checksum verification;
- Clang ASan/UBSan tests and replay verification.

The Windows MSVC build, native tests, and replay fixture remain visible as a
manual, non-blocking job because no Windows runner is currently available. The
job is not treated as passed.

## Local definition checks

The YAML parses successfully, its stage and clean-cache settings were inspected
programmatically, generated output is current, all six generator tests pass,
and both packaging scripts pass ShellCheck.

## Pending external evidence

This audit verifies the pipeline definition, not a GitLab pipeline execution.
The validation matrix must not mark GitLab CI green until a pipeline completes
on the configured runners.
