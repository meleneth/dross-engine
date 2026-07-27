# Dross Engine Working Agreement

This file is concise by design. Detailed rules live in `docs/` and accepted ADRs.

## Working behavior

- Inspect the repository before asking questions.
- Preserve accepted architecture unless a concrete contradiction is demonstrated.
- Prefer reversible decisions, narrow public APIs, small composable units, and deterministic tests.
- Complete the active phase. Do not leave placeholders, fake implementations, TODO-driven architecture, or knowingly dead registrations.
- Make small commits with one reason to change. Keep the working tree clean at phase boundaries.
- Run formatting, static analysis, tests, and the exact validation commands named by the active phase.
- Report exactly what changed and what was validated.

## Source control

- Refuse to begin implementation in a dirty repository unless the dirt is clearly part of the requested work.
- Pull with a non-destructive, fast-forward-only strategy when a remote and upstream are configured.
- Never rewrite unrelated history.
- Add `.gitattributes` before meaningful source creation, normalize existing files with `git add --renormalize .`, and preserve LF for source, CMake, Markdown, Python, GDScript, Godot text resources, and CI files. Windows `.bat` and `.cmd` files may use CRLF.

## Architecture rules

- `dross_core` has no Godot headers, Godot types, scene assumptions, or dynamic calls into GDScript.
- EnTT is the ECS storage and query mechanism.
- eventpp is the domain event transport.
- Boost.Ext.SML is used for known compile-time lifecycle and capability FSMs.
- `pcg64` behind `RandomHub` is the authoritative random source.
- Authoritative simulation is fixed-tick and single-threaded.
- Authoritative movement, occupancy, range, and facing use the Dross hex model.
- Godot is an adapter, authoring environment, and presentation runtime. Godot transforms, physics, navigation, signals, and animations never decide domain truth.
- GDScript submits commands, performs queries, contributes declarative rules, and reacts to immutable events. It cannot mutate the registry or components.
- Commands are requests. Events are immutable facts. Logs are neither.
- Expected command failures are represented as typed results, not exceptions.
- A rejected command must leave authoritative state unchanged.
- Animation completion may release presentation gating, but it may not decide whether an action occurred.
- Stable content and API shapes are typed. Do not use `Dictionary` payloads for established commands, events, or definitions.
- Runtime-authored ECS component types are forbidden. New component storage and capabilities require C++.
- Do not rely on unordered iteration, raw pointer order, `std::hash`, standard-library random distributions, wall-clock time, or floating-point physics for deterministic outcomes.
- Do not serialize raw EnTT storage, Godot nodes, GDScript object graphs, callables, or active stacks.

## No speculative architecture

An abstraction must gain a production consumer in the same phase that introduces it. Delete dead buses, empty interfaces, unused event types, and compatibility shims that have no live emitter or listener. Future seams belong in documentation until a phase exercises them.

## Testing rules

- Write the failing test first for behavior.
- Unit-test plain C++ without launching Godot whenever possible.
- Use property-based or exhaustive tests for hex math, footprints, path invariants, deterministic ordering, serialization, and replay.
- Test each SML transition, guard, unexpected event policy, snapshot, and restore path.
- Use headless Godot tests only for the actual Godot boundary.
- Treat project warnings as errors where supported. Mark dependencies as system dependencies rather than weakening project warnings.
- A screen rendering correctly is not evidence that the domain model is correct.

## Stop conditions

Stop before changing architecture when:

- an accepted dependency cannot satisfy the required behavior;
- Godot or godot-cpp compatibility invalidates the pinned baseline;
- cross-platform deterministic tests diverge;
- a phase requires a domain concept explicitly marked deferred;
- satisfying a task would require direct GDScript component mutation or Godot-owned authoritative state;
- a save migration would discard unknown or required data;
- a proposed shortcut creates a second movement, event, identity, or state model;
- a phase cannot produce a live production consumer for a new abstraction.

Explain the contradiction, show the smallest reproducer or failing test, and propose an ADR. Do not silently substitute a different architecture.
