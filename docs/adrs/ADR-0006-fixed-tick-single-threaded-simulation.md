# ADR-0006: Fixed-Tick, Single-Threaded Authoritative Simulation

Status: Accepted

## Context

Replay, event ordering, scripting, and turn transitions are easier to reason about when mutation has one serialized timeline. Godot render frames and physics cadence are presentation concerns.

## Decision

Run authoritative simulation on one thread in fixed integer ticks. The initial default is 30 ticks per second. Godot accumulates wall time and requests ticks, while replay advances ticks directly.

## Consequences

- real-time exploration remains discrete under the hood;
- expensive work may require later read-only jobs;
- no locks are needed in the initial core mutation path;
- tick rate becomes save and replay metadata.

## Enforcement

- core APIs accept ticks, not frame delta;
- mutation from adapter threads is queued;
- no wall-clock reads in authoritative code;
- concurrency requires a later profiling-backed ADR.
