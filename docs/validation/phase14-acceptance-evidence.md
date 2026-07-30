# Phase 14 Acceptance Evidence

Recorded 2026-07-29 through commit `73468fc`.

This document maps the first vertical slice acceptance checklist to executable
evidence. A checked item means the available Linux and Godot validation proves
that criterion. It does not imply Windows or Steam Deck device coverage.

| Acceptance criterion | Evidence |
| --- | --- |
| No Godot include in `dross_core` | `architecture.no_godot_in_core` scans the core dependency boundary. |
| EnTT stores authoritative entities and components | The ECS world, identity, occupancy, movement, combat, door, save, and replay tests exercise the production EnTT registry; the architecture audit found no second authoritative store. |
| eventpp carries live domain events with real listeners | `accepted placement commits before a queued typed event is observed`, `event subscriptions are lifetime safe and listener phases are fixed`, combat lifecycle, movement fact, and door lifecycle tests exercise live emitters and listeners. |
| SML controls distinct lifecycles | World, mode, movement, combat, and door transition, rejection, snapshot, and restore tests exercise their production state machines. |
| Editor and runtime grids use the same compiled map | The Phase 10 Godot geometry, resource, and aggregate integration scripts compile the editor-authored map and consume it through the runtime overlay. |
| Footprint and facing are authoritative | Occupancy, traversal, movement, save, and canonical-hash tests operate on authoritative `HexPose` and footprint state. |
| Asymmetric multi-cell footprint tests pass | `asymmetric footprint rotates to traverse a narrow route`, rotation/expansion tests, and `multi-cell footprint occupancy moves atomically at the edge boundary`. |
| GDScript cannot mutate components | The generated typed script ports buffer rule contributions, state writes, and deferred commands; boundary inspection found no registry or mutable component exposure. |
| Command rejection is side-effect free | Placement, ability, movement identity, combat identity, callback-fault, and failed-load tests compare unchanged authoritative state after rejection. |
| Animation cannot cause or prevent domain mutation | Door acknowledgement and timeout tests prove presentation does not alter committed truth; the Godot Phase 12 and 14 scripts exercise presentation from committed events. |
| Save and load preserve every required state | Component, RNG, machine, movement, combat, door, script state, full-world, checkpoint, and three vertical-slice boundary round trips pass. |
| Replay matches canonical hashes | Replay checkpoint, divergence-localization, committed fixture, repeated recording, save-resume, and full Thump scenario verification pass on Linux. |
| Generators are clean and idempotent | `./bin/dross generate all`, `./bin/dross check generated`, and all six generator/schema pytest cases pass without a diff. |
| Linux GCC, Linux Clang, sanitizer, Windows, and Godot headless CI pass | Open: GCC, Clang ASan/UBSan, and all eight Godot headless scripts pass locally. LeakSanitizer, Windows, and Steam Deck hardware remain unverified as recorded in the validation matrix. |
| No introduced abstraction is unused | All seven architecture tests pass; the source audit found no orphan event, implementation file, private header, dead bus, empty production interface, unused machine, placeholder production path, unclassified active dependency, or packaging mapping drift. |

## Aggregate commands

```sh
ctest --preset linux-debug --output-on-failure
cmake --build --preset linux-debug --target tidy

ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=print_stacktrace=1 \
  ctest --preset linux-asan-ubsan --output-on-failure

./bin/dross generate all
./bin/dross check generated
PYTHONPATH=tools/dross_cli/src .venv/bin/python -m pytest -q

godot --headless --path godot -s res://tests/run_all.gd
```

Detailed command results and environment exceptions are recorded in
`phase14-validation-matrix.md`.
