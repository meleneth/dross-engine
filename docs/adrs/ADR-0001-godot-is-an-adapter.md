# ADR-0001: Godot Is an Adapter and Authoring Environment

Status: Accepted

## Context

Godot provides valuable rendering, animation, UI, audio, import, scene, export, and editor systems. Its node and signal model does not supply the authoritative ECS, deterministic event phases, compile-time FSMs, or replay guarantees required by Dross.

## Decision

Dross keeps a Godot-free authoritative C++ core. Godot is used for authoring and presentation through a dedicated adapter target.

Godot nodes, transforms, physics, navigation, signals, animations, and resource objects do not become authoritative simulation state.

## Consequences

- most domain tests run without Godot;
- a translation layer is required;
- presentation can be rebuilt after load;
- content definitions compile from Godot resources into core DTOs;
- engine development pays explicit adapter cost in exchange for auditability and portability.

## Enforcement

- `dross_core` cannot include or link godot-cpp;
- architecture tests scan core sources;
- Godot-facing values are wrapper classes or conversions;
- phase acceptance requires headless proof before visual proof.
