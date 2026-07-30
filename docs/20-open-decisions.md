# Open and Deferred Decisions

These questions are acknowledged but intentionally not solved by the first implementation phases. Codex must not fill them with quick local conventions.

## Runtime author state machines

Dross eventually needs easy, copyable game-facing FSM examples for encounters, quests, dialogue, and entity behavior.

Unresolved requirements include:

- validated dynamic definitions;
- GDScript guards and actions;
- deterministic event and transition ordering;
- visual editor representation;
- persistence and migrations;
- mod override rules;
- debugging and trace integration;
- a relationship to built-in SML machines;
- author ergonomics without dictionary soup.

Decision point: after the first slice proves several native SML machines and several GDScript event and rule scripts.

Forbidden interim solution: string state plus a large `match` callback presented as the engine's runtime FSM architecture.

## Final save container codec

The persistence architecture uses explicit DTOs, component codecs, versions, migrations, and canonical ordering from the start. The final external container may remain readable JSON, use MessagePack or CBOR, or move to another established schema format.

Decision point: after current snapshot size, migration needs, and load-time measurements exist.

The codec choice may not leak into capability APIs.

## General pathfinding library

The required graph is an implicit, dynamic graph of `HexPose` states with footprints, rotation, edges, occupancy, and policy-dependent costs. A generic graph library may reduce algorithm maintenance or may create more adapter complexity than it removes.

Decision point: phase 03 or movement planning spike, based on a tested asymmetric-footprint example.

Requirements either way:

- use established containers;
- deterministic tie breaking;
- optimality tests;
- no custom heap;
- planner interface isolated from movement execution.

## Runtime line of sight and cover

The grid and edge model reserves sight and semantic facts, but the first slice does not define cover, partial obstruction, projectile arcs, or multi-height visibility.

Decision point: after movement and Thump prove the map representation.

## Dialogue, inventory, quests, reputation, and schedules

These are expected capabilities, not first-slice placeholders. Do not create empty event types, components, or managers for them.

Decision point: each receives its own vertical slice and ADRs.

Phase 15 proposes a combined ThumpDemo dialogue, inventory, and quest slice in
ADR-0025. That ADR intentionally does not resolve general authored state
machines, branching quest graph formats, reputation, or schedules.

## Multi-region streaming

`RegionId` and stable references are foundational, but the first world loads one region. Streaming, unloaded entity representations, and cross-region commands are deferred.

Decision point: after one region saves, reloads, and replays correctly.

## Parallel work

Authoritative simulation remains single-threaded. Read-only jobs, editor bakes, asset loading, and presentation work may later use concurrency.

Decision point: only after profiling identifies a real budget problem and deterministic merge semantics are designed.

## Public engine distribution and naming clearance

Dross is used as the repository and engine name. Formal public trademark clearance, packaging, documentation website, and third-party native ABI policy are deferred until public distribution becomes real.

## Orthographic versus restrained perspective

The first camera is orthographic. Camera code should not assume the authoritative grid depends on projection. A restrained perspective option may be evaluated after the first room is legible.

## Godot binding baseline upgrade

The baseline uses Godot 4.7.1 with the Godot 4.7 stable API from the immutable
godot-cpp revision accepted by ADR-0023. Moving to another godot-cpp revision
requires an explicit dependency update.
