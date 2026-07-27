# Dross Engine Codex Specification

This directory is the implementation contract for **Dross Engine powered by Godot**.

Dross is a deterministic, hex-grid-centered RPG simulation engine written in modern C++. Godot supplies rendering, animation, audio, UI, content import, scene authoring, and editor integration. Dross owns authoritative world state, movement, combat, finite state machines, event ordering, persistence, deterministic random numbers, replay, and the API exposed to GDScript.

The first proving game slice is intentionally tiny:

- one arbitrary 3D room;
- one visible logical hex grid;
- one player actor;
- one field mouse;
- one door that is not on the required route to combat;
- real-time exploration;
- turn-based combat;
- one ability named `Thump`;
- one GDScript-authored reaction;
- save, reload, and deterministic replay.

The small surface is not permission to build disposable internals. Every implemented path must establish the architecture that later content will use.

## How to use this corpus

1. Place `AGENTS.md`, `docs/`, and `CODEX-START.md` at the repository root.
2. Give Codex the text from `CODEX-START.md`.
3. Codex must read the charter, invariants, relevant ADRs, and the active phase before changing code.
4. Codex works through phases in numeric order.
5. It continues to the next phase after all exit criteria pass, unless a stop condition is encountered.
6. Each phase is completed through small, tested commits. A phase is not complete merely because something appears on screen.

## Reading order

1. `AGENTS.md`
2. `docs/00-charter.md`
3. `docs/01-architectural-invariants.md`
4. `docs/02-domain-language.md`
5. `docs/03-repository-and-build.md`
6. `docs/04-capability-architecture.md`
7. `docs/05-ecs-world-model.md`
8. `docs/06-hex-grid-model.md`
9. the remaining architecture documents as referenced by the active phase
10. `docs/phases/README.md`
11. the active phase brief

## Status conventions

- **Locked** means Codex may not reinterpret the decision during implementation.
- **Deferred** means the problem is acknowledged, but implementation is forbidden until a named phase or ADR.
- **Stop condition** means Codex must stop before committing an architectural substitution.
- **Production consumer** means a behavior exercised by the running slice, not merely a test fixture or unused registration.

## Project identity

```text
Engine name:          Dross Engine
Descriptive form:     Dross Engine powered by Godot
Repository name:      dross
C++ namespace:        dross
Godot class prefix:   Dross
CMake target prefix:  dross_
Python command:       ./bin/dross
Default content IDs:  dross:<name>
```

## Non-goals for the first implementation sequence

- web or mobile builds;
- networked simulation;
- multithreaded authoritative world mutation;
- hostile-code sandboxing for mods;
- a general runtime statechart language for authors;
- an entire dialogue, inventory, or quest product;
- physically continuous authoritative movement;
- Godot physics or navigation as the source of movement truth;
- ABI stability for third-party native extensions.

The documents describe later-safe seams, but phases must not create unused abstraction museums. An abstraction enters code only when its phase gives it a real caller and a real test.
