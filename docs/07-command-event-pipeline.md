# Command and Event Pipeline

## Overview

Dross separates requests, validation, mutation, facts, reactions, and presentation.

```text
input, AI, replay, or GDScript
              |
              v
        typed command queue
              |
              v
   structural validation and lookup
              |
              v
 native and scripted rule contribution
              |
              v
      capability-specific plan
              |
              v
       no-fail ECS commit
              |
              v
      immutable domain events
              |
        +-----+------+----------------+
        |            |                |
        v            v                v
 native reactions  script reactions  presentation projection
        |            |
        +------ typed follow-up commands
                       |
                       v
              later command phase
```

No arrow points from presentation back into committed truth.

## Command envelope

Each command has stable metadata:

```cpp
struct CommandEnvelopeHeader {
    CommandId id;
    Tick target_tick;
    CommandSource source;
    CausationId causation;
    CorrelationId correlation;
    CommandTypeId type;
};
```

The payload is a generated typed value. The envelope may use a generated `std::variant` internally, but handlers receive the concrete command type.

Initial command sources:

```text
player input
GDScript
authoritative system
replay
headless test
```

The source does not grant mutation authority. Every path uses the same handler and rules.

## Command router

Commands have exactly one authoritative handler. Use a generated command registry or router that:

- maps stable `CommandTypeId` to one handler;
- rejects duplicate registration at engine construction;
- validates payload type;
- returns a typed or erased `CommandResult` suitable for traces;
- does not use eventpp multicast semantics for commands.

Eventpp is for facts with multiple listeners. A command is not a broadcast vote.

## Validation stages

Command processing occurs in distinct stages.

### 1. Envelope validation

- command type is registered;
- target tick is allowed;
- source is allowed to request the command;
- referenced world instance matches;
- payload satisfies generated structural constraints.

### 2. Entity and definition lookup

- entity references are valid;
- required capabilities exist;
- content definitions are loaded;
- aliases resolve unambiguously.

### 3. Base rules

The capability computes its native baseline, such as movement cost, attack range, or door interaction requirements.

### 4. Rule contribution

Contributors run in deterministic phases:

1. engine invariants;
2. capability-native rules;
3. region script;
4. entity scripts in stable `EntityId` order;
5. encounter or quest scripts in stable module ID order;
6. global scripts in stable module ID order.

Each contribution records source, operation, value, and reason. Numeric callback priorities are forbidden in the initial API.

### 5. Plan creation

The handler creates a capability-specific immutable plan. A rejection here contains structured reasons and does not mutate world state.

### 6. Commit

Applying an accepted plan performs only operations already proven valid. It mutates capability-owned components and appends immutable events to a collector.

If an unexpected failure occurs during commit, the world enters `Faulted`; Dross does not pretend the command was cleanly rejected after partial mutation.

## Rule contribution

Rule query objects are accumulators, not world handles.

Example GDScript-facing intent:

```gdscript
func contribute_attack_rules(
        query: DrossAttackRuleQuery,
        ctx: DrossScriptContext) -> void:
    if ctx.query.has_trait(query.target, &"sacred_mouse"):
        query.reject(&"mousecult:sacrilege")
```

C++ translates each call into a typed contribution. The query object exposes only operations valid for that rule family.

Do not expose a universal `set_value(key, variant)` rule bag.

Rule resolution specifies:

- whether multiple restrictions accumulate;
- modifier ordering and arithmetic;
- replacement precedence;
- minimum and maximum clamps;
- reason retention for UI and traces;
- behavior when a script faults.

Use rational or fixed-point arithmetic where percentages affect authoritative outcomes.

## Domain event queue

Use eventpp for typed queued domain events. The schema generator provides stable IDs and the dispatch registration needed by eventpp.

The selected eventpp form must support heterogeneous typed payloads without converting stable APIs into dictionaries. A phase may use generated wrappers around `HeterEventQueue` or a generated variant plus strongly typed subscription helpers, but the public capability API remains typed.

Required queue behavior:

- append events during commit;
- drain only at explicit phase boundaries;
- preserve deterministic emission order;
- identify event type and event instance;
- attach tick, causation, correlation, and source command;
- prohibit mutation of queued payloads;
- trace listener phase and resulting follow-up commands;
- reject subscription changes while a queue is actively draining, or defer them to a safe boundary.

## Reaction phases

For each command cycle:

```text
A. resolve accepted command and collect events
B. publish native invariant reactions
C. publish native capability reactions
D. publish GDScript reactions by scope order
E. project presentation events
F. append follow-up commands to the next eligible queue
G. record trace and optional canonical state hash
```

A reaction may observe earlier facts in the same event batch but may not invoke a command handler recursively.

Follow-up commands include causation metadata pointing to the event that created them.

## Event semantics

Emit facts at useful granularity. For example, a successful attack may emit:

```text
AbilityCommitted
AttackResolved
DamageApplied
ActorKilled
ActionPointsSpent
```

Do not emit events merely because a function ran. Events should represent stable domain facts that other capabilities or scripts can rely on.

A command rejection is recorded in command trace and may produce a presentation response, but it does not emit `AttackFailed` unless failure itself is a committed game fact with downstream meaning.

## Idempotence and duplicate commands

The initial local game does not require distributed exactly-once processing, but replay and UI retries benefit from command identity.

The runtime keeps a bounded record of completed `CommandId` values for the current session. A duplicate command ID returns the previously recorded result or a typed duplicate rejection according to policy. It never applies twice.

## Script faults

A script fault during rule collection occurs before commit, so the command is rejected with `script_fault` and the world remains unchanged.

A script fault during post-commit reaction cannot undo the committed fact. Dross therefore:

1. records the fault with script scope and event causation;
2. queues no partial output from the failing callback;
3. transitions the world lifecycle to `Faulted` after the current event batch reaches a safe boundary;
4. disables save and further commands;
5. surfaces a developer diagnostic.

Do not continue a corrupted authoritative run for convenience.

## First-slice command and event inventory

Minimum live commands:

```text
SpawnEntity
MoveTo
CancelMovement
RequestCombatStart
EndTurn
PerformAbility
OpenDoor
CloseDoor
SaveWorld
LoadWorld
```

Minimum live events are introduced only when consumed:

```text
EntitySpawned
MovementStarted
ActorEnteredCell
MovementCompleted
CombatStarted
TurnStarted
ActionPointsSpent
AbilityCommitted
DamageApplied
ActorKilled
DoorOpened
DoorClosed
WorldSaved
WorldLoaded
```

If an event has no production consumer and no external API requirement in its phase, omit it until it does.
