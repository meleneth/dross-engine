# Capability Architecture

## Purpose

A capability is the standard unit for adding new authoritative behavior to Dross. Examples include movement, doors, combat, inventory, dialogue, reputation, or status effects.

A capability is not required to inherit from a runtime base class. It is a package of owned concepts and registrations assembled explicitly at the composition root.

## Capability package

A mature capability may contain:

```text
capabilities/<name>/
├── components.hpp
├── definitions.hpp
├── commands.generated.hpp
├── events.generated.hpp
├── queries.hpp
├── rules.hpp
├── plans.hpp
├── system.hpp
├── system.cpp
├── machine.hpp
├── persistence.hpp
├── persistence.cpp
├── godot_bindings.hpp
├── godot_bindings.cpp
└── tests/
```

A phase creates only the files exercised by live behavior.

## Ownership contract

Each capability documents:

- components it owns;
- components it may read;
- commands it handles;
- queries it answers;
- rules it contributes;
- events it emits;
- events it reacts to;
- machine instances it owns;
- persistence records it encodes;
- Godot definitions or API types it exposes;
- deterministic ordering requirements;
- failure and invariant policies.

Only the owner mutates its components. Another capability submits a command or invokes a read-only query.

## Explicit composition root

The engine has one visible composition root that installs built-in capabilities and their generated registries.

Conceptually:

```cpp
EngineRuntime build_engine(const EngineConfig& config) {
    EngineBuilder builder{config};

    install_identity(builder);
    install_hex_world(builder);
    install_movement(builder);
    install_doors(builder);
    install_combat(builder);
    install_scripting(builder);

    return std::move(builder).build();
}
```

Do not hide registration in static initializers. Static initialization order is not an acceptable dependency system or deterministic ordering mechanism.

The composition root may be generated from the capability manifest once the generator exists, but its output remains readable and explicit.

## Command handling pattern

A capability handler follows a plan and commit shape:

```cpp
Result<MovePlan, MoveRejection> plan_move(
    const WorldRead& world,
    const MoveTo& command,
    const MoveRules& rules);

void apply_move_plan(
    WorldWrite& world,
    const MovePlan& plan,
    DomainEventCollector& events);
```

Rules and validation complete before `apply_*` begins. Applying an accepted plan does not return an expected domain failure. A failure there indicates an invariant or infrastructure fault.

Do not create a generic mutation DSL before at least two capabilities prove a common need. Capability-specific plans are allowed and preferred over an abstract transaction framework with no real consumer.

## Query pattern

Queries are read-only and typed. They return values or immutable views, not mutable references.

Examples:

```cpp
Result<HexPose, EntityLookupError> pose_of(EntityRef entity) const;
bool has_trait(EntityRef entity, ContentId trait) const;
TraversalAssessment assess_traversal(EntityRef actor, HexPose to) const;
```

Queries may compose other queries, but they must not trigger mutation, dispatch events, advance random streams, or depend on presentation state.

## Event reaction pattern

A capability subscribes to immutable domain events through eventpp during installation. Reaction callbacks may:

- update capability-owned derived state during their declared native reaction phase;
- enqueue typed follow-up commands;
- project presentation events;
- record observability data.

They may not invoke an active command handler recursively.

Every subscription must have a live emitter and a test proving its behavior. Registration without a production emitter is dead architecture and must not be committed.

## FSM integration

A capability that has a real lifecycle provides:

- SML transition table;
- typed event adapter;
- machine host or scoped machine instance;
- logger adapter;
- snapshot type;
- restore path;
- transition matrix tests;
- unexpected-event behavior.

A capability does not receive an FSM merely because FSMs are a design focus. A stable stateless calculation remains a function.

## Persistence integration

Each persistent component or machine state has:

- stable type ID;
- schema version;
- explicit DTO;
- encode and decode functions;
- migration chain;
- fixture tests;
- ownership in one capability.

A capability may not serialize another capability's private component representation.

## Godot exposure

A capability exposes only authoring and scripting concepts needed by content:

- typed definition resources;
- immutable event wrappers;
- command methods;
- query methods;
- rule query objects;
- editor inspectors or gizmos when necessary.

The adapter translates and validates all inputs. Godot exposure does not make the adapter the owner of the capability.

## Capability generator

The Python scaffolder eventually supports:

```bash
./bin/dross generate capability doors
./bin/dross generate command doors OpenDoor actor:entity_ref door:entity_ref
./bin/dross generate event doors DoorOpened door:entity_ref actor:entity_ref
./bin/dross generate machine doors DoorLifecycle
./bin/dross generate resource doors DoorDefinition
```

The generator creates compileable skeletons, tests, schemas, and explicit registration changes. It does not invent domain semantics.

## Capability completion checklist

A capability is complete for a phase only when:

- its production path uses every introduced abstraction;
- component ownership is documented;
- accepted and rejected commands are tested;
- emitted events have live consumers where required;
- ordering is deterministic;
- save and restore behavior is covered if state is persistent;
- headless behavior is proven;
- Godot exposure is tested only if the phase needs it;
- no direct component mutation bypass exists through GDScript or presentation;
- the generated and handwritten portions have a clear boundary.
