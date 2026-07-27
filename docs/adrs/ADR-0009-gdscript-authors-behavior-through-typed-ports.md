# ADR-0009: GDScript Authors Behavior Through Typed Ports

Status: Accepted

## Context

The game should be authored mostly in GDScript while C++ owns fundamental capabilities. Fallout-style scripting power is useful, but direct component mutation would recreate temporal soup.

## Decision

Expose typed definition resources, command APIs, query APIs, rule query objects, immutable event wrappers, deterministic random access, and script state bags to GDScript.

GDScript may contribute rules and react to events. It cannot define ECS component storage or mutate components directly.

## Consequences

- bindings and examples are generated from schemas;
- script durable state is explicit;
- script callbacks run in deterministic scope order;
- script faults have world-fault semantics rather than silent continuation.

## Enforcement

- stable APIs avoid dictionaries;
- typed callback tests run in Godot headless mode;
- source scans discourage direct Godot RNG in authoritative scripts;
- adding component storage requires C++.
