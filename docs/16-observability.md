# Observability and Debugging

## Goal

A systemic RPG produces long causal chains. Dross must explain not only what state exists, but how a command, rule, event, machine, script, and random decision produced it.

Observability is structured engine behavior, not scattered print statements.

## Trace model

A `TraceSink` receives typed trace records from the authoritative runtime.

Initial sink implementations:

```text
NullTraceSink
InMemoryTraceSink
JsonLinesTraceSink
GodotTraceSink
```

The interface exists when the first command pipeline uses both null and in-memory sinks. Do not add unused exporters early.

## Command trace

Each command trace records:

```text
tick
command ID and type
source
causation and correlation IDs
structural validation result
entity and definition lookup result
ordered rule contributions
final resolved rule values
plan summary
acceptance or rejection
rejection reason and details
emitted domain event IDs
follow-up command IDs
canonical state hash checkpoint when enabled
```

Sensitive or huge payloads may have structured summaries, but identifiers and reasons remain machine-readable.

## Event trace

Each event record includes:

```text
tick
event ID and type
source command
payload summary or canonical payload
listener phase
listener identity
queued follow-up commands
script scope and callback when applicable
```

The trace must distinguish domain events from presentation events and log annotations.

## FSM trace

The SML logger adapter records:

```text
machine family
scope identity
input event
guard name and result
source state
destination state
action name
unexpected event policy
causation metadata
```

Machine names and state IDs are stable generated or explicit identifiers, not compiler-specific type-name strings in persisted traces.

## RNG trace

Authoritative RNG may record in developer mode:

```text
stream ID
call sequence number
operation
bounds or probability
result
causation ID
```

Replay does not consume recorded results. RNG traces exist to find accidental stream coupling and first divergence.

## Script trace

Record:

```text
module ID
scope identity
callback name
event or rule query identity
commands and state writes appended
random calls
fault details and stack
execution duration as non-authoritative telemetry
```

Execution duration is never part of deterministic state or ordering.

## State inspection

Provide canonical inspection APIs for:

- entity component summary by `EntityId`;
- current hex pose and footprint expansion;
- occupancy evidence;
- active machine state and snapshot;
- script modules and state bag;
- random stream state and call count;
- pending commands and event queue depth;
- current combat order and AP;
- cell and edge provenance from bake and overrides.

Godot developer UI and headless CLI both consume the same core inspection DTOs.

## Replay divergence

On mismatch, the replay runner reports:

1. first differing tick;
2. external command being processed;
3. accepted or rejected result difference;
4. first domain event difference;
5. first machine transition difference;
6. first random stream call difference if traced;
7. canonical state section and record that diverged.

A single final hash mismatch without localization is not sufficient tooling.

## Metrics

Performance and count metrics may include:

```text
tick duration
commands processed and rejected
events emitted
script callbacks
path nodes expanded
entities and components by type
save size and duration
editor bake cells and duration
view count
```

Metrics are non-authoritative. They use a separate telemetry path and cannot alter simulation ordering.

## Logs versus events

- Domain events are immutable game facts.
- Trace records explain engine causality.
- Logs describe infrastructure and developer diagnostics.
- Presentation signals notify Godot consumers.

Do not use a log line as the only evidence that another capability should have received.

## First-slice developer view

The initial Godot developer panel should eventually show:

- current tick and mode;
- selected entity identity, pose, footprint, health, AP, and machines;
- hovered cell and edge facts with bake or override reasons;
- last command result;
- recent domain events;
- script callback and fault information;
- master seed and replay recording status;
- current canonical state hash.

The panel is useful but not required before the underlying inspection DTO has a headless test.
