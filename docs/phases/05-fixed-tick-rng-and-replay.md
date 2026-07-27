# Phase 05: Fixed Tick, RandomHub, Trace, and Replay Skeleton

## Goal

Add the single-threaded fixed-tick runtime, named `pcg64` streams, deterministic sampling, canonical state hashing, command recording, and a replay runner for the existing command scenario.

## Read first

- `docs/09-rng-fixed-tick-replay.md`
- `docs/16-observability.md`
- ADR-0006, ADR-0007, ADR-0014

## Live production consumer

Extend the command-event headless scenario so one rule uses a named deterministic random roll and commands are scheduled by tick. Record the scenario, replay it, and verify checkpoint hashes.

## Scope

Implement:

```text
SimulationClock
EngineRuntime tick loop
external and follow-up command queues by tick
RandomHub and RandomStream
stable stream derivation
integer and rational sampling wrappers
TraceSink with Null and InMemory implementations
canonical core snapshot writer for current components
BLAKE3 checkpoint hash
ReplayHeader and external command log
replay command in dross_headless
```

Do not implement full save/load yet. Replay begins from a constructed fixture snapshot or canonical scenario seed state.

## Fixed-tick phases

Define and test the initial tick phase order:

```text
ingest external commands for tick
process command cycles and queued follow-ups within budget
finish domain event and script-port phases
advance time-based systems with no hidden mutation
produce presentation or inspection snapshot
record checkpoint hash when configured
increment tick
```

Set explicit budgets for command and reaction loops to detect infinite event-command cycles. Exceeding the budget faults the runtime with a causal trace.

## RandomHub

- master seed is explicit;
- named streams are created lazily and deterministically;
- stream derivation uses stable bytes and a versioned algorithm;
- PCG state and stream selector are snapshot-ready;
- no capability accesses PCG directly;
- no standard distributions or `std::shuffle`;
- script child streams are modeled even though GDScript uses them later.

## Canonical state

Hash the authoritative state currently implemented:

- tick;
- identity allocator;
- entities by EntityId;
- current components by stable type ID;
- occupancy in canonical form or rebuilt-equivalent representation;
- random streams by ID;
- pending external commands allowed by policy.

Do not hash trace buffers or presentation data.

## Replay file

The first replay format includes:

```text
engine and replay schema version
scenario or initial snapshot identity
minimal content manifest containing the real base package ID
master seed
random algorithm version
external commands with target ticks
expected checkpoint hashes
```

Use explicit DTOs even though the full save registry arrives in phase 07.

## Tests first

- tick scheduling order;
- external and follow-up command phase behavior;
- command loop budget fault;
- PCG golden vectors;
- named stream derivation golden vectors;
- stream independence;
- integer range and rational chance behavior;
- shuffle deterministic vector;
- RandomHub snapshot and restore;
- canonical hash independent from storage insertion order;
- record and replay identical result;
- changed seed changes the known randomized fact;
- first divergence reports tick and section;
- forbidden random API source scan.

## Prohibited shortcuts

- frame delta inside core;
- one global random stream for everything;
- replaying recorded random outputs;
- hashing raw component memory;
- using JSON object iteration without canonical ordering;
- an unlimited follow-up command loop;
- time-based test sleeps;
- presentation RNG consuming authoritative streams.

## Validation

```bash
cmake --build --preset linux-debug
ctest --preset linux-debug --output-on-failure
dross_headless scenario command-event-kernel --seed 12345 --record build/kernel.dross-replay
dross_headless replay --verify-checkpoints build/kernel.dross-replay
```

Run the replay at least twice and compare output. Run sanitizer and both GCC and Clang jobs where available.

## Suggested commits

1. `test: specify fixed tick and command scheduling`
2. `feat: add single-threaded simulation runtime`
3. `test: lock PCG and stream golden vectors`
4. `feat: add RandomHub and deterministic sampling`
5. `feat: record and verify replay checkpoints`

## Exit criteria

- same seed and external commands reproduce state hashes;
- command order is tick-based and explicit;
- RandomHub is the only authoritative RNG path;
- trace and replay have real production consumers;
- divergence is localized;
- no full save architecture was faked prematurely.

## Stop conditions

- PCG outputs differ across supported compilers;
- BLAKE3 or canonical encoding differs across platforms;
- a current authoritative value cannot be represented without floating-point nondeterminism;
- event-command cycles need a domain policy not yet defined.
