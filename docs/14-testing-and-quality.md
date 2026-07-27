# Testing and Quality Strategy

## Testing pyramid

Dross is tested at the lowest boundary that can prove a behavior.

```text
many plain C++ unit and property tests
fewer core integration and headless scenario tests
focused GDExtension and GDScript integration tests
small editor workflow and rendered smoke tests
```

Do not launch Godot to test hex rotation, combat cost, event ordering, save migration, or an SML guard.

## C++ test framework

Use Catch2 v3 for unit, integration, and scenario tests. Use RapidCheck for generative property tests where shrinking adds value. Exhaustive bounded tests remain appropriate for small hex domains and state transition spaces.

Tests are registered with CTest and run through CMake presets.

## Test organization

```text
tests/unit/             one class, value type, handler, or machine behavior
tests/property/         generated or exhaustive invariants
tests/integration/      several core capabilities with real event flow
tests/scenario/         headless player-visible behavior
tests/fixtures/         committed saves, schemas, maps, and replay logs
tests/architecture/     dependency and source-boundary checks
godot/tests/            GdUnit4 and headless Godot tests
```

Test file locations should mirror capability ownership.

## Test-first behavior

For each behavioral change:

1. express the desired outcome in a failing test;
2. implement the minimum complete production path;
3. refactor while tests remain green;
4. add rejection and invariant cases;
5. inspect for unused abstractions;
6. run the phase validation suite.

A compile-only placeholder is not an implementation step unless the phase explicitly establishes build plumbing.

## Value-type tests

Strong value types require:

- construction and validation;
- comparison and stable ordering;
- overflow or invalid-range behavior;
- serialization round trips;
- canonical string form where applicable;
- hash golden vectors when used in deterministic identity.

## Hex properties

At minimum:

- cube invariant always holds;
- axial to cube to axial round trip;
- distance symmetry and triangle inequality over bounded samples;
- six rotations return the original coordinate and footprint;
- neighbor followed by opposite direction returns origin;
- projection and inverse quantization agree on cell centers;
- canonical edge identity is direction-independent;
- footprint placement has no duplicate cells;
- path cost is optimal on exhaustive small maps;
- equivalent maps with different insertion order produce the same path;
- asymmetric footprints never cross blocked cells while rotating or moving.

## Command and event tests

Each command handler needs:

- accepted path;
- every documented rejection class;
- proof that rejection leaves the canonical state unchanged;
- emitted event payload and order;
- causation and correlation metadata;
- deterministic rule ordering;
- GDScript contribution behavior through a fake script runtime;
- no reentrant command execution;
- duplicate command policy;
- trace records.

## FSM tests

Each SML machine needs:

- every legal transition;
- each guard false path;
- each action through a fake dependency;
- entry and exit behavior when used;
- unexpected event policy;
- snapshot and restore from every persistent state;
- logger output shape;
- integration with its real command or event source.

Do not assert only that an enum changed. Assert the invariant represented by the state.

## Persistence tests

- current save round trip;
- byte-identical canonical payload for equivalent state;
- committed fixture migration for every older schema;
- missing required content failure;
- unknown required component failure;
- invalid reference failure without mutating the active world;
- random and machine state restoration;
- save during allowed exploration and combat boundaries;
- save refusal during a non-quiescent phase;
- uninterrupted versus save-reload final state hash equality.

## Replay tests

- same seed and input command log produce identical event and state hashes;
- platform golden vectors for RNG and canonical hash;
- changed seed changes a known random branch;
- first divergence reporting identifies the tick and state section;
- replay regenerates native and GDScript follow-up commands;
- forbidden nondeterministic APIs are absent from authoritative sources.

## Godot and GDScript tests

Use GdUnit4, pinned to a compatible release, for GDScript and scene-level tests.

Required categories:

- extension loads headlessly;
- registered classes exist;
- definition resources validate and compile;
- typed GDScript callbacks receive typed wrappers;
- command API cannot expose direct mutation;
- script state survives save and reload;
- view interpolation does not change core pose;
- animation acknowledgement cannot apply domain effects;
- editor plugin loads and preserves overrides;
- runtime grid overlay is derived from the compiled map.

## Architecture tests

Automate rules that should never depend on review memory:

- source scan forbids Godot includes under core;
- CMake link graph forbids `dross_core -> godot-cpp`;
- source scan forbids `std::rand`, standard distributions, `std::shuffle`, Godot RNG, and `std::hash` in authoritative paths;
- source scan forbids raw `entt::entity` in public Godot bindings;
- schemas and generated outputs are clean;
- every registered command has one handler;
- every registered event subscription points to a live event type;
- every persistent component has a codec and version;
- every stable public GDScript event is typed;
- no core API accepts `void*`, `Variant`, or generic dictionaries.

Source scans support architecture, but behavior tests remain required.

## Static analysis

- `clang-format` is mandatory.
- `clang-tidy` runs with a curated checked-in configuration.
- warnings are errors for Dross targets.
- include-what-you-use may be added after the baseline build is stable.
- dependency warnings are isolated through system include paths.

Do not blanket-disable a warning because one generated or third-party file emits it. Scope suppressions narrowly and document them.

## Sanitizers

Linux CI includes:

- ASan plus UBSan in a debug-like build;
- TSan once concurrency enters any relevant adapter or tooling path;
- leak detection where compatible with Godot headless execution.

Core tests should be sanitizer-clean from the first implementation phase.

## Coverage

Coverage reveals untested branches and migration paths. It is not a substitute metric for design quality.

Publish line and branch reports for `dross_core`, excluding generated bindings and dependencies. Do not set a vanity percentage that rewards trivial tests while critical invariants remain unproven.

## Performance tests

No early microbenchmark is allowed to justify bypassing architecture. Add Catch2 benchmarks or a dedicated benchmark target when a measurable budget exists.

Useful later budgets include:

- path preview latency on representative maps;
- tick cost by capability;
- script dispatch count and time;
- save size and latency;
- canonical state hash time;
- editor bake time.

Correctness and traceability come first.

## Phase completion report

Codex reports:

```text
commits created
files and architecture changed
tests added
test commands and results
format and static analysis results
sanitizer results
warnings or skipped targets
remaining risks
reason for continuing or stopping
```

No vague `tests pass` without naming the commands.
