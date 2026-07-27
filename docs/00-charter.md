# Dross Engine Charter

## Mission

Dross Engine is a reusable C++ simulation engine for authored, systemic, hex-grid-centered role-playing games. It is designed to be used with Godot, not swallowed by Godot.

The engine should make two workflows natural:

1. **Authoring a game** mostly through Godot scenes, typed resources, editor tooling, and GDScript.
2. **Adding a new fundamental capability** mostly through C++, with a standard path for components, commands, events, rules, FSMs, persistence, Godot exposure, tests, and documentation.

The project is called **Dross Engine powered by Godot** when the complete relationship matters.

## Why Dross exists

Godot already solves rendering, animation, UI, audio, import, scene authoring, and export. It does not natively provide the specific architectural guarantees Dross requires:

- a data-oriented authoritative world in EnTT;
- typed domain event queues with explicit resolution phases;
- compile-time lifecycle FSMs in Boost.Ext.SML;
- deterministic, cross-platform simulation from a fixed seed and command stream;
- a logical hex topology independent of 3D presentation;
- a GDScript surface that cannot bypass domain invariants;
- explicit save codecs and migrations instead of serializing ambient engine state.

Dross exists to supply that kernel and a disciplined bridge to Godot.

## Design priorities

In priority order:

1. Correct and inspectable causality.
2. Deterministic behavior and reproducible failures.
3. Testability without Godot.
4. Strong boundaries between simulation, scripting, and presentation.
5. An authoring workflow that does not require C++ for ordinary world content.
6. A repeatable C++ capability pattern.
7. Refactorability and schema evolution.
8. Performance appropriate to an isometric RPG.
9. Convenience.

Convenience is welcome when it does not counterfeit one of the higher priorities.

## First proof

The engine is not proven by compiling or drawing a model. It is proven when the first vertical slice demonstrates, through both headless and Godot tests:

- geometry-derived and manually overridden visible hex cells;
- authoritative hex position and facing;
- real-time exploration using fixed-tick edge traversal;
- a pending transition into turn-based combat;
- individual initiative and action points;
- a typed `Thump` command and resulting events;
- a field mouse behavior authored in GDScript;
- a door represented as an edge-anchored entity, located off the required combat route;
- C++ state committed independently from animation completion;
- save and reload of world, door, actor, script, FSM, tick, and RNG state;
- replay from the same snapshot, seed, and command stream to the same canonical state hashes.

## Supported targets

The initial supported runtime targets are:

- Windows desktop;
- Linux desktop;
- Steam Deck through the Linux build.

Web and mobile are excluded from the initial architecture contract.

## Scope boundaries

### Dross owns

- authoritative identities and entity lifetimes;
- ECS components and systems;
- fixed simulation ticks;
- deterministic random streams;
- logical hex cells, edges, footprints, occupancy, movement, and facing;
- commands, rules, events, and phase ordering;
- capability FSMs and simulation modes;
- combat and action resolution primitives;
- script context, command, query, rule, and event contracts;
- save snapshots, codecs, migrations, replay, and state hashes;
- authoring data compiled from Godot into runtime inputs.

### Godot owns

- rendering and cameras;
- model, texture, animation, audio, and UI assets;
- scene composition and editor UX;
- presentation nodes and visual interpolation;
- selection raycasts and non-authoritative visual collision;
- import and platform export.

### GDScript owns

- map-specific and entity-specific authored behavior;
- encounter, quest, dialogue, and special-case reactions built from existing capabilities;
- declarative rule contributions;
- command submission and typed event reactions;
- editor workflow code where C++ engine math is not required;
- presentation-local behavior.

GDScript does not own component storage, authoritative mutation, event ordering, identity, random algorithms, save encoding, or domain invariants.

## Development principle

Start as the engine is meant to continue, but do not confuse that with implementing unused machinery early. Each phase must introduce only the architecture needed by a live behavior, then test that architecture at the smallest meaningful scale.
