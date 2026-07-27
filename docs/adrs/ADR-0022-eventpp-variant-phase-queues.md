# ADR-0022: Typed Variant Events over Phase-Specific eventpp Queues

Status: Accepted

## Context

Phase 04 needs heterogeneous domain events, immutable typed listener APIs,
queued delivery, and a fixed reaction phase order. eventpp offers both
`HeterEventQueue` and conventional `EventQueue`. The heterogeneous form keys
listeners by callback signature, which does not preserve the schema's stable
event identity by itself and makes phase registration less explicit.

## Decision

Generate a closed `DomainEventPayload` variant from authored event schemas and
place strongly typed subscription helpers in front of phase-specific
`eventpp::EventQueue` instances.

For the first live event, the internal queue item contains the generated
`EntityPlaced` value and immutable command causation metadata. Queue keys are
private implementation details. Public listeners receive
`const EntityPlaced&`; no dictionary, `void*`, or mutable registry handle
crosses the boundary.

The kernel enqueues during commit and drains queues only after command handling
has ended, in this order:

1. native invariants;
2. native capability reactions.

Later script and presentation phases extend this explicit sequence. Listener
registration is rejected while draining. Follow-up commands are appended to
the next command-cycle queue.

## Consequences

- eventpp provides live queued delivery and lifetime-safe listener handles.
- Generated variants keep stable event types visible and compile checked.
- Phase order is structural rather than a numeric callback priority.
- Adding an event requires an authored schema and generated typed adapter.

## Enforcement

- event payload parameters are const references;
- event queues are drained only at an explicit kernel boundary;
- subscription handles remove listeners on destruction;
- no command handler may be invoked from an event callback;
- registration cannot mutate an actively draining listener set.
