# ADR-0003: eventpp Carries Domain Events

Status: Accepted

## Context

Domain facts require queued multicast delivery, deterministic phases, and multiple consumers. Godot signals are runtime object relationships and are unsuitable as the authoritative event backbone.

## Decision

Use eventpp for queued typed domain event delivery. Commands use a separate single-handler router.

Events are immutable facts emitted after commit. Reactions enqueue later commands and cannot reenter active command resolution.

## Consequences

- a generated typed event registry is required;
- event phases and ordering become visible architecture;
- Godot signals remain useful only at the presentation boundary;
- dead event registrations must be removed.

## Enforcement

- every event type has a schema and stable ID;
- every subscription has a live emitter and test;
- no direct mutation from event callbacks outside the declared native reaction phase;
- no dictionary event payloads for stable APIs.
