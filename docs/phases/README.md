# Implementation Phases

Phases are executed in numeric order. Codex may continue automatically after a phase when every exit criterion passes and no stop condition appears.

## Phase rule

A phase introduces the smallest complete production path that exercises its architecture. It may document future extension points, but it may not commit unused interfaces, dead events, placeholder handlers, or empty capability packages.

## Sequence

| Phase | Proof |
|---|---|
| 00 | reproducible warning-clean repository and CI foundation |
| 01 | strong core value types and deterministic utilities |
| 02 | EnTT world, stable identity, and entity lifetime |
| 03 | pointy-top hex topology, footprints, edges, and path planning |
| 04 | schema generator, command router, eventpp queue, and rule phases |
| 05 | fixed tick, PCG RandomHub, traces, canonical hashes, and replay skeleton |
| 06 | SML world and simulation-mode machines with snapshot and restore |
| 07 | explicit save container, component codecs, and fresh-world load |
| 08 | GDExtension loads and exposes typed definition resources and host |
| 09 | typed GDScript query, command, rule, event, state, and RNG ports |
| 10 | editor grid region, geometry bake, overrides, compile, and visible overlay |
| 11 | fixed-tick real-time exploration movement on the authoritative hex graph |
| 12 | turn-based combat, Thump, optional side door, animation-safe projection |
| 13 | complete save, reload, replay, and cross-platform scenario equivalence |
| 14 | integrated Godot vertical slice, hardening, documentation, and releaseable prototype baseline |
| 15 | ThumpDemo dialogue, inventory, and quest behavior through typed native facts and GDScript orchestration |

## Continuation policy

Proceed to the next phase only when:

- all named validation commands pass;
- the phase has at least one production consumer for each introduced abstraction;
- the working tree is clean after commits;
- no accepted ADR was bypassed;
- generated output is clean;
- the phase report identifies no structural surprise.

A flaky test, unsupported compiler behavior, dependency mismatch, or Godot limitation is not a reason to weaken the test or boundary. It is a stop condition to investigate.
