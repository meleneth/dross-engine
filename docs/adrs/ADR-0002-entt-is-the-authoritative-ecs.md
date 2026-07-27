# ADR-0002: EnTT Is the Authoritative ECS

Status: Accepted

## Context

The game world contains actors, doors, containers, effects, and many logical entities that do not always have visible nodes. A data-oriented component model supports composition, queries, persistence, and headless tests.

## Decision

Use EnTT for entity and component storage. Components are data, systems own behavior, and capabilities own mutation of their components.

Do not create a second homegrown ECS or represent components as Godot child nodes.

## Consequences

- `entt::entity` remains an internal ephemeral handle;
- persistent IDs and indexes are required;
- system ordering must be explicit where outcomes depend on it;
- Godot views are optional projections of ECS entities.

## Enforcement

- no EnTT handles in public or Godot APIs;
- one mutation owner documented per component;
- no runtime GDScript-defined component storage;
- persistence uses explicit component codecs, not EnTT snapshots.
