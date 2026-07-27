# ADR-0004: Boost.Ext.SML Owns Known FSMs

Status: Accepted

## Context

World lifecycle, simulation mode, movement, combat, and door behavior have explicit states and transitions. Ad hoc enums, booleans, and switch statements obscure invalid transitions.

## Decision

Use Boost.Ext.SML for known compile-time engine and capability machines. Machines are capability-specific, logged, tested, snapshotted, and restored through explicit production paths.

A runtime author statechart system is deferred.

## Consequences

- machine host and persistence patterns are required;
- SML internals are not serialized;
- not every calculation becomes an FSM;
- author-defined dynamic machines need a later design rather than a false SML abstraction.

## Enforcement

- transition tables use typed states and events;
- each machine defines unexpected-event policy;
- persistent states round-trip through tests;
- universal entity-state enums are forbidden.
