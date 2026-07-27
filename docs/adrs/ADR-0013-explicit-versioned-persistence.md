# ADR-0013: Persistence Uses Explicit Versioned DTOs

Status: Accepted

## Context

Serializing raw ECS or Godot object state makes refactoring, migration, and validation fragile. Save and load are required in the first slice.

## Decision

Every persistent capability defines versioned DTOs, codecs, migrations, and canonical hash behavior. Load validates into plans and constructs a fresh world before swap.

## Consequences

- persistence code is intentional work for each capability;
- in-memory components can refactor independently from save shape;
- fixtures become long-lived compatibility tests;
- a codec can evolve without changing capability APIs.

## Enforcement

- raw registry, object graph, pointer, and stack serialization are forbidden;
- unknown required records fail loudly;
- migrations have committed fixtures;
- save occurs only at quiescent boundaries.
