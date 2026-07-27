# ADR-0016: Organize Domain Code by Capability Ownership

Status: Accepted

## Context

Large horizontal folders such as `managers`, `components`, or `systems` hide ownership and make cross-feature changes sprawl. Dross needs an obvious path for adding a capability.

## Decision

Organize core domain behavior by capability, with shared foundation packages only for genuinely shared concepts. A capability owns its components, handlers, rules, machines, persistence, bindings, and tests.

## Consequences

- some technical concepts appear in several capability folders;
- ownership becomes reviewable;
- generators can create coherent feature slices;
- public APIs remain narrow.

## Enforcement

- no generic manager dumping ground;
- every component names one owner;
- cross-capability mutation uses commands;
- shared abstractions require at least two live consumers.
