# FSM Architecture

## Principle

Dross uses Boost.Ext.SML for known, compile-time engine and capability lifecycles. FSMs are not decorative wrappers around enums. A machine exists when transitions, guards, entry or exit behavior, unexpected events, or lifecycle observability justify it.

## Initial machine families

The first implementation sequence requires these machine families:

### World lifecycle

```text
Empty
Loading
Ready
Running
Saving
Unloading
Faulted
```

### Simulation mode

```text
Exploration
CombatPending
Combat
```

### Movement lifecycle

```text
Idle
Planning
Traversing
Completing
Blocked
Cancelled
```

The exact state set may be reduced if tests prove a state has no distinct invariant or behavior. Do not add states only to mirror function names.

### Combat session

```text
Inactive
Starting
ActorTurn
ResolvingAction
Ending
```

### Door lifecycle

The authoritative door machine begins with domain states such as:

```text
Closed
Open
Locked
```

Opening and closing animation may have presentation states, but the domain door does not wait for animation to become true.

## Capability-specific machines

There is no universal `EntityState` machine. A character may simultaneously have:

- movement state;
- combat participation state;
- status effect machines;
- dialogue or encounter state;
- presentation animation state.

These machines communicate through typed commands and events, not shared boolean flags.

## Machine host

Behavior-heavy SML objects do not become anonymous ECS data. A capability owns a `MachineHost` that maps stable scope identity to machine instances and coordinates them with ECS components.

Conceptual interface:

```cpp
template<class MachineDefinition, class ScopeId>
class MachineHost {
public:
    Result<void, MachineCreateError> create(ScopeId scope, MachineSnapshot initial);
    Result<MachineOutcome, MachineError> process(ScopeId scope, const MachineEvent& event);
    Result<MachineSnapshot, MachineLookupError> snapshot(ScopeId scope) const;
    Result<void, MachineRestoreError> restore(ScopeId scope, const MachineSnapshot& snapshot);
    void erase(ScopeId scope);
};
```

The actual implementation may be specialized per machine family. Do not create a type-erased universal machine framework before two live machine families demonstrate the common contract.

## State snapshots

Every persistent machine has a stable explicit snapshot DTO. The snapshot stores the domain state and required context, not SML internal memory.

Example:

```cpp
struct MovementMachineSnapshot {
    MovementStateId state;
    std::optional<PathId> path;
    std::uint32_t next_transition_index;
    Tick transition_started_at;
};
```

Restore constructs a fresh SML machine and drives an explicit typed restore event or selected initialization path to the saved state. Using SML testing-only state injection in production is forbidden.

Every state must have a round-trip test:

```text
construct -> enter state -> snapshot -> destroy -> restore -> same observable state
```

## External state and ECS

A machine may refer to ECS data through injected narrow dependencies. Its actions submit mutations through the owning capability's plan or explicit phase. It does not retain raw registry pointers across calls.

If machine context must be persistent, it belongs in the snapshot DTO or an owned ECS component with one source of truth. Do not duplicate the same state in an enum component and an SML instance without a synchronization contract.

## SML logger

All machines use a Dross SML logger adapter that records:

- machine family;
- scope identity;
- current tick;
- processed event type;
- guard name and result;
- transition source and destination;
- action name;
- unexpected event;
- causation and correlation IDs when applicable.

Logging is routed through `TraceSink`. It is compiled or configured cheaply enough to remain available in development builds.

## Unexpected events

Each machine defines a policy:

- `Ignore` only when the event is explicitly harmless and traced;
- `Reject` when it represents an invalid command transition;
- `Fault` when it proves an engine invariant violation.

A catch-all that silently swallows events is forbidden.

Tests must cover the policy from every relevant state.

## Testing machines

Use both ordinary SML instances and SML testing support where appropriate for transition-table unit tests. Production restore must not depend on testing policies.

Required test styles:

- table-driven transition tests;
- guard true and false paths;
- action side effects through fakes;
- unexpected event tests;
- snapshot and restore from every persisted state;
- trace record tests;
- property tests for legal event sequences where useful;
- integration tests showing commands and domain events drive the machine.

## Engine-facing examples for authors

Dross will eventually provide readable examples showing how authored behavior corresponds to engine machines. For example, a GDScript entity script may react to `DoorOpened` without owning the door lifecycle, while a future runtime encounter machine may expose states such as `Dormant`, `Active`, and `Resolved`.

The author-facing runtime machine system is deliberately deferred. It is difficult because it must support:

- dynamic definitions;
- guards and actions in GDScript;
- validation before load;
- deterministic ordering;
- persistence and migration;
- debugging and visualization;
- mod compatibility;
- examples that are easy to copy and modify.

Do not solve this early with dictionaries, string-based switch statements, or one script callback per state. See `docs/20-open-decisions.md`.
