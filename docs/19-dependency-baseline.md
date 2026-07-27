# Dependency Baseline

This is the initial technology baseline as of 2026-07-26. Phase 00 records exact immutable tags or commit hashes in CMake and the Python lock file.

## Runtime and editor

### Godot 4.7.1 stable

Role: editor, rendering, animation, UI, audio, import, scenes, and platform export.

Reference: https://godotengine.org/download/archive/4.7.1-stable/

Godot is installed as a tool, not acquired through CPM.

### godot-cpp 4.5 stable API target

Role: official C++ GDExtension bindings.

Reference: https://github.com/godotengine/godot-cpp

Use the stable Godot 4.5 binding line with Godot 4.7.1 initially. Configure the extension compatibility minimum accordingly. The master v10 line is beta at this baseline date and is not the default.

## Core architecture

### EnTT 3.16.x

Role: ECS storage, views, and related utilities.

Reference: https://github.com/skypjack/entt

Use an exact stable tag. Do not wrap EnTT into a homegrown ECS.

### Boost.Ext.SML 1.2.x

Role: compile-time FSMs.

Reference: https://github.com/boost-ext/sml

This is not an official Boost library. Use its testing and logging facilities where appropriate.

### eventpp

Role: typed event queues and dispatch.

Reference: https://github.com/wqking/eventpp

Pin an exact tag or reviewed commit. Phase 03 must prove the chosen heterogeneous typed event integration before broad schema generation.

### pcg-cpp

Role: authoritative `pcg64` random streams.

Reference: https://github.com/imneme/pcg-cpp

Pin an exact reviewed commit and add golden-vector tests. All access goes through `RandomHub`.

## Foundation utilities

### tl::expected

Role: recoverable `Result<T, E>` behavior under C++20.

Reference: https://github.com/TartanLlama/expected

Dross exposes only the `dross::Result` alias, not the dependency type in documentation or GDScript.

### nlohmann/json

Role: initial readable DTO and manifest encoding where JSON is suitable.

Reference: https://github.com/nlohmann/json

Core persistence remains archive and DTO based so the container codec can evolve.

### BLAKE3

Role: canonical state, content, and manifest hashes.

Reference: https://github.com/BLAKE3-team/BLAKE3

Use the official implementation and stable byte encoding.

### fmt

Role: core formatting for diagnostics and traces.

Reference: https://github.com/fmtlib/fmt

Do not mix Godot `String` into core diagnostics.

### spdlog

Role: infrastructure logging adapter where useful.

Reference: https://github.com/gabime/spdlog

Structured command and event traces remain Dross types rather than spdlog messages.

## Build and tests

### CMake and CPM.cmake

Role: build generation and pinned dependency acquisition.

References:

- https://cmake.org/
- https://github.com/cpm-cmake/CPM.cmake

CPM is a thin FetchContent wrapper. Pin CPM itself and every acquired package.

### Catch2 3.x

Role: C++ tests and focused benchmarks.

Reference: https://github.com/catchorg/Catch2

### RapidCheck

Role: property-based tests with shrinking.

Reference: https://github.com/emil-e/rapidcheck

If current compiler integration is broken, that is a phase 00 stop condition. Do not silently replace property tests with a few hand-selected examples.

### GdUnit4 6.x

Role: GDScript, resource, scene, and editor integration tests.

Reference: https://github.com/godot-gdunit-labs/gdUnit4

Pin a release compatible with the chosen Godot baseline.

## Python scaffolding

### Python 3.11+

Use a locked `pyproject.toml` environment with:

```text
Typer
Pydantic
Jinja2
PyYAML
pytest
```

A tool such as `uv` may manage the lock file, but the repository entry point remains `./bin/dross` and CI documents the exact bootstrap.

## Upgrade policy

A dependency upgrade requires:

1. exact version change;
2. upstream release-note review;
3. full tests and generated-code check;
4. replay golden-vector verification;
5. save fixture verification when serialization can change;
6. Godot headless validation for Godot or godot-cpp changes;
7. an ADR when the change alters a public or architectural contract.

Do not track moving branches.
