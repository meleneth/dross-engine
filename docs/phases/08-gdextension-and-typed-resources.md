# Phase 08: GDExtension Boundary and Typed Resources

## Goal

Build and load `dross_godot`, register a world host and the first typed definition resources, and prove conversion into Godot-free core DTOs.

## Read first

- `docs/11-godot-boundary.md`
- `docs/19-dependency-baseline.md`
- ADR-0001, ADR-0010, ADR-0017, ADR-0020, ADR-0023

## Live production consumer

A minimal Godot project loads the extension headlessly, creates a `DrossWorldHost`, instantiates typed footprint and actor definition resources, compiles them into core DTOs, starts the current synthetic world, advances ticks, and queries a deterministic inspection result.

## Dependency step

Add the pinned godot-cpp revision carrying the Godot 4.7 stable API through
CPM or the documented CMake subdirectory integration. Use Godot 4.7.1 stable
as the tested editor and runtime.

The top-level project owns C++ standard, MSVC runtime, warnings, and output paths. Isolate any godot-cpp CMake global-setting behavior rather than letting it leak unnoticed.

## Scope

Implement:

```text
dross_godot shared target
dross.gdextension descriptor
register_types.cpp and initialization levels
DrossWorldHost Node
DrossContentId validation helper or wrapper
DrossFootprintDefinition Resource
DrossActorDefinition Resource with only fields needed soon
core DTO compilation and validation
headless Godot smoke scene and GdUnit4 bootstrap
extension copy or output path into godot/bin
```

Do not implement the complete script runtime or editor plugin yet.

## World host

The host owns a `std::unique_ptr<EngineRuntime>`. It exposes only lifecycle and inspection methods needed by the smoke test.

The host advances fixed ticks using an accumulator but the test can call a deterministic explicit tick method available only in development or test configuration.

No authoritative autoload singleton.

## Resource compilation

Resources expose Inspector-editable properties but compile into core values. Validation errors include resource path and property name.

The core does not retain Godot `Ref` values.

## Tests first

C++ adapter tests where possible, then Godot headless tests:

- extension loads in Godot 4.7.1;
- minimum compatibility metadata is correct;
- classes exist in ClassDB;
- typed resources instantiate;
- valid ContentId and footprint compile to core DTO;
- invalid footprint or ID produces structured error;
- changing or freeing a Resource after compilation does not mutate core DTO;
- world host advances deterministic ticks;
- deleting a view or Resource cannot destroy an authoritative entity;
- `dross_core` still configures without godot-cpp.

## Prohibited shortcuts

- Godot types added to core headers;
- Resource object stored as authoritative definition;
- raw `Variant` dictionaries compiled lazily during command handling;
- autoload singleton owning registry;
- SCons build for Dross extension;
- adopting an unpinned godot-cpp branch;
- suppressing Dross warnings globally because of godot-cpp.

## Validation

```bash
cmake --preset linux-debug
cmake --build --preset linux-debug
ctest --preset linux-debug --output-on-failure
godot --headless --path godot --editor --quit
godot --headless --path godot -s res://tests/run_phase08.gd
```

Use the exact pinned Godot executable in CI.

## Suggested commits

1. `build: add pinned godot-cpp GDExtension target`
2. `test: load Dross classes in headless Godot`
3. `feat: add world host and typed definition resources`
4. `test: prove resource compilation isolates core state`

## Exit criteria

- extension loads under pinned Godot;
- core remains Godot-free;
- typed resources compile into plain DTOs;
- one host runs the real fixed-tick core;
- headless Godot tests are in GitLab CI;
- no script or editor abstractions were added unused.

## Stop conditions

- required class, Resource, or editor integration is unavailable through the pinned 4.7 bindings;
- godot-cpp CMake integration corrupts top-level compiler or CRT policy;
- extension cannot load on both Linux and Windows target layouts;
- Resource inheritance cannot provide the typed authoring shape without dynamic dictionaries.
