# ADR-0008: Commands and Queries Protect Mutation

Status: Accepted

## Context

GDScript and cross-capability code need meaningful power without direct access to mutable ECS state. Direct mutation bypasses validation, events, persistence, and causal traces.

## Decision

External and cross-capability mutation uses typed commands. Reads use typed queries. Command handlers validate and build plans before a no-fail commit. Events are emitted after commit.

Responsible systems may mutate owned components during commit or an explicit native phase. The registry is not exposed as a general service.

## Consequences

- command schemas and routers are core infrastructure;
- common UI and script actions travel the same path as replay;
- command rejection is side-effect free;
- capability-specific plans are preferred over a speculative generic transaction DSL.

## Enforcement

- no setters for components in GDScript APIs;
- command tests compare state before and after rejection;
- only one handler per command;
- cross-capability direct writes fail architecture review.
