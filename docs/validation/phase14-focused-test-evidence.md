# Phase 14 Focused Test Evidence

Date: 2026-07-29

This record maps every focused test named by the Phase 14 brief to executable
coverage.

| Required case | Executable evidence |
| --- | --- |
| Full headless scenario | `headless.thump_save_checkpoints` runs the complete authoritative Thump scenario without Godot. |
| Full Godot scenario | `res://tests/run_phase14.gd` drives exploration, combat transition, door, Thump, scripts, save/load, views, and diagnostics. |
| Save and reload at required boundaries | Native boundary tests plus the headless exploration, combat, and completed-save inspections; Phase 14 saves and restores exploration and completed combat state. |
| Replay fixture on CI targets | Linux Clang, Linux GCC, sanitizer, and manual Windows GitLab jobs verify `tests/fixtures/thump-v1.dross-replay`. |
| Missing animation and animation timeout | Native and Godot door tests withhold presentation acknowledgement, release the gate by timeout, reject late acknowledgement, and preserve committed door truth. |
| View deletion and recreation | Phase 11 frees a registered entity view, verifies stale lookup removal, recreates the same stable identity, and reconstructs presentation from a snapshot. |
| Rebake preserves override | Native bake merge tests and the Phase 10 Godot geometry test preserve manual traversability and provenance across rebake. |
| Invalid content blocks load with useful diagnostics | Save decoding, component validation, transactional load-plan tests, and the Phase 14 truncated-save test reject invalid data, expose an error, and preserve the current canonical hash. |
| Mod manifest mismatch blocks save load | Manifest ordering/hash tests and changed-or-missing required-content load tests reject the mismatch before world construction. |
| Fixed frame-rate variations do not alter state | Phase 11 advances two authoritative hosts with different counts of presentation frames and compares the resulting authoritative cell and lifecycle state. |
| Clean build from empty dependency cache | The GitLab GCC release job disables the shared cache and configures, builds, tests, and verifies replay. Pipeline execution is still pending. |
| Generated output clean | Regeneration, drift check, six generator/schema tests, and an empty Git diff pass locally and are encoded in GitLab CI. |
| Packaging smoke launch | The Linux package workflow exports outside the source tree, smoke-launches, rejects runtime errors, and verifies all artifact checksums. |

## Focused rerun

The native acceptance subset passed 11 of 11 tests. Godot Phase 10, 11, 12,
and 14 integration scripts also passed. The complete aggregate suite results
remain recorded in `phase14-validation-matrix.md`.

Windows execution is not implied by this mapping. Its CI job and package
workflow remain manual and unverified until a Windows runner is available.
