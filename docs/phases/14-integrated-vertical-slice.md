# Phase 14: Integrated Vertical Slice and Hardening

## Status

**Active.** Phase 14 began without access to Windows or Steam Deck hardware.
Those targets remain explicitly unverified and are not release claims.
Available Linux compiler, sanitizer, replay, and Godot validation remains part
of this phase.

Current recorded evidence is in
`docs/validation/phase14-validation-matrix.md`. Windows, Steam Deck device, and
LeakSanitizer validation remain explicitly unverified.

## Goal

Assemble the complete Godot demo, remove remaining rough architectural edges, validate every acceptance criterion, and leave Dross as a clean foundation for the next real game capability.

## Read first

- `docs/18-first-vertical-slice.md`
- all accepted ADRs
- every prior phase report

## User-visible result

A player launches the demo and can:

1. see an orthographic isometric 3D room;
2. reveal or use the visible hex grid;
3. hover cells and preview a path;
4. move in real-time exploration;
5. inspect and open the door installed in the dividing wall;
6. cross the doorway to approach the field mouse;
7. enter turn-based combat;
8. move and spend AP;
9. use Thump;
10. observe the scripted mouse reaction or death;
11. save and reload;
12. record or replay the deterministic scenario through developer tooling.

## Hardening scope

This phase may refine existing architecture only where integration exposes a concrete issue. It must not add unrelated features.

Required work:

- complete Godot scene and content package organization;
- generated API and example documentation;
- developer diagnostics panel for tick, mode, selected entity, cell facts, last command, events, script callback, seed, and hash;
- clean error surfaces for invalid content and save load;
- packaging scripts for Linux and Windows prototype builds;
- Steam Deck smoke procedure and result;
- performance baseline for tick, path preview, script callbacks, save, and grid bake;
- source and dependency license inventory;
- final architecture audit for dead abstractions and boundary leaks;
- complete README for building, testing, authoring, and adding a capability;
- one copyable C++ capability example and one GDScript behavior example based on live code.

## Architecture audit

Search explicitly for:

- Godot types in core;
- direct component mutation outside owners;
- event types without live emitters or listeners;
- SML candidates replaced by boolean or switch lifecycle logic;
- SML machines with no real production transitions;
- logs standing in for domain events;
- UI or animation mutating simulation;
- duplicate movement, path, identity, or state models;
- raw strings where ContentId is required;
- forbidden random APIs;
- unordered semantic iteration;
- save omissions;
- script member state treated as durable;
- placeholder or TODO production paths;
- generated code drift.

Delete or correct findings before declaring the slice complete.

## Tests

Run the entire suite plus focused end-to-end tests:

- full headless scenario;
- full Godot scenario;
- save and reload at required boundaries;
- replay fixture on CI targets;
- missing animation and animation timeout;
- view deletion and recreation;
- rebake preserves override;
- invalid content blocks load with useful diagnostics;
- mod manifest mismatch blocks save load;
- fixed frame-rate variations do not alter state;
- clean build from empty dependency cache on at least one CI job;
- generated output clean;
- packaging smoke launch.

## Performance recording

Record baselines, not premature hard limits:

```text
headless tick median and high percentile
path preview on demo and larger synthetic map
script callbacks per event and duration
save and load duration and bytes
canonical hash duration
editor bake cells per second
```

No optimization may bypass invariants. Any optimization changes receive tests proving identical results.

## Documentation

Final repository docs must answer:

- how to configure, build, and test on Linux and Windows;
- how to open the Godot project;
- how to create a map region, bake, override, and compile the grid;
- how to create a typed definition Resource;
- how to write an entity or region GDScript;
- how to add a C++ command, event, Resource, FSM, codec, and Godot binding through the generator;
- how to record and verify replay;
- how to inspect save failures;
- which decisions are still deferred.

Examples are tested or parsed in CI.

## Prohibited shortcuts

- adding feature breadth to make the demo look more game-like;
- accepting a manual-only step that should be encoded in build or editor tooling;
- hiding a failed target from CI;
- claiming deterministic support on an untested platform;
- leaving dead architecture because it might be useful later;
- polishing presentation while a core acceptance item is missing.

## Validation

Run every supported preset and CI-equivalent command. At minimum:

```bash
./bin/dross generate all
./bin/dross check generated
cmake --build --preset linux-debug
ctest --preset linux-debug --output-on-failure
cmake --build --preset linux-asan-ubsan
ctest --preset linux-asan-ubsan --output-on-failure
dross_headless scenario thump-on-field-mouse --seed 12345 --record build/final.dross-replay
dross_headless replay --verify-checkpoints build/final.dross-replay
godot --headless --path godot -s res://tests/run_all.gd
```

Build prototype packages and run them outside the source tree.

## Suggested commits

Use small commits aligned to concrete hardening findings. The final commit may be:

`docs: declare Thump on Field Mouse vertical slice complete`

only after every checklist item is evidenced.

## Exit criteria

Every checkbox in `docs/18-first-vertical-slice.md` is satisfied with a test, build, trace, or documented manual smoke result. The working tree is clean. CI is green. Generated output is clean. The architecture audit finds no live violation and no dead abstraction.

## Stop conditions

- any platform remains nondeterministic;
- a required user workflow depends on editing generated files;
- a core boundary violation is defended as temporary;
- the next capability cannot be added without replacing a foundational model;
- packaging exposes a godot-cpp compatibility or runtime-linking issue that requires a new ADR.
