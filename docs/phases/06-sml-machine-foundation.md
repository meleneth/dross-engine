# Phase 06: SML Machine Foundation

## Goal

Implement real Boost.Ext.SML machines for world lifecycle and simulation mode, including tracing, unexpected-event policies, snapshot, restore, and integration with the fixed-tick runtime.

## Read first

- `docs/08-fsm-architecture.md`
- ADR-0004

## Live production consumer

The headless runtime must use the machines to:

- construct an empty engine;
- load the current synthetic world;
- enter running exploration;
- request combat pending and then combat through typed events;
- return to exploration;
- snapshot and restore machine state;
- enter faulted on a forced script or loop fault fixture.

No placeholder machine exists solely in tests.

## Scope

Implement:

```text
WorldLifecycle SML machine
SimulationMode SML machine
Dross SML logger adapter
machine scope identity where needed
explicit snapshot DTOs
production restore events or initialization paths
runtime gating based on machine state
inspection DTO for current states
```

Do not implement movement, combat-turn, or door machines until their live phases.

## World lifecycle

Required transitions include:

```text
Empty + BeginLoad -> Loading
Loading + LoadSucceeded -> Ready
Loading + LoadFailed -> Faulted
Ready + BeginRun -> Running
Running + BeginSave -> Saving
Saving + SaveSucceeded -> Running
Saving + SaveFailed -> Faulted or Running according to explicit tested policy
Running + BeginUnload -> Unloading
Unloading + UnloadSucceeded -> Empty
operational state + FatalFault -> Faulted
```

Choose and document save failure semantics before implementation. A save I/O failure that leaves world state intact may return to Running, while invariant failure faults. Use distinct events rather than one ambiguous failure.

## Simulation mode

Required behavior:

```text
Exploration + CombatRequested -> CombatPending
CombatPending + SafeBoundaryReached -> Combat
Combat + CombatEnded -> Exploration
```

Unexpected duplicate requests are explicitly rejected or ignored with trace according to state.

## Snapshot and restore

Snapshots use stable state IDs and any required context. Restore builds new machines and processes supported restore initialization. Do not use testing-only `set_current_states` in production.

## Tests first

- every legal transition;
- guard false paths;
- exact unexpected-event policy from each state;
- logger records stable machine, state, event, guard, and action IDs;
- snapshot and restore from every persistent state;
- runtime rejects commands when world is not Running;
- combat request waits for safe boundary event;
- fatal fault prevents later commands and saves;
- replay regenerates identical machine trace;
- no duplicated state enum is independently mutable.

## Prohibited shortcuts

- enum plus switch standing in for SML;
- one giant machine for all engine behavior;
- machine state mirrored in an unrelated mutable component;
- silent unexpected events;
- SML testing policy for production restore;
- string states used as authoritative IDs;
- movement or door states added before live behavior.

## Validation

```bash
cmake --build --preset linux-debug
ctest --preset linux-debug --output-on-failure
dross_headless scenario lifecycle-machines --record build/lifecycle.dross-replay
dross_headless replay --verify-checkpoints build/lifecycle.dross-replay
```

## Suggested commits

1. `test: specify world lifecycle transitions and restore`
2. `feat: add traced SML world lifecycle machine`
3. `test: specify exploration and combat mode transitions`
4. `feat: add simulation mode machine to runtime`

## Exit criteria

- runtime behavior genuinely depends on SML states;
- every machine state restores;
- trace uses stable IDs;
- invalid transitions are visible and tested;
- only two live machines exist;
- no universal dynamic machine framework was introduced.

## Stop conditions

- SML production restore cannot be implemented without unsupported internals;
- logger type information cannot be mapped to stable IDs cleanly;
- world lifecycle requires persistence behavior that contradicts phase 07 design.
