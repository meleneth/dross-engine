# Domain Language

Use these terms consistently in code, tests, documentation, and GDScript APIs.

## Identity

### `ContentId`

A validated namespaced identifier such as `dross:thump` or `mousecult:sacred_mouse`. It identifies definitions and authored concepts. The canonical string is retained. A cached hash may accelerate lookup but is never the sole identity.

### `EntityId`

A stable persistent identity for one world entity. It is distinct from an EnTT handle and is not reused within a world lineage.

### `EntityAlias`

An optional human-authored `ContentId` that points to an entity placed in content, such as `demo:side_door`. Aliases are convenient references, not the underlying identity.

### `EntityRef`

A validated reference exposed across subsystem or script boundaries. It identifies a world instance and stable `EntityId`. It becomes invalid when the entity no longer exists. It never exposes an EnTT handle.

### `WorldInstanceId`

Identifies one loaded authoritative world instance. It prevents a reference from one load or replay being accepted by another.

## Time and requests

### `Tick`

An integer authoritative simulation step.

### `Command`

A typed request to change the world. A command includes identity, source, target tick, payload, and causation metadata. It may be rejected without mutation.

### `CommandPlan`

A capability-specific, fully validated description of the mutation that an accepted command will perform. Planning may fail. Applying an accepted plan is the no-fail commit portion.

### `CommandResult`

A typed accepted or rejected outcome. Rejections contain machine-readable reason IDs and structured details suitable for UI and traces.

### `DomainEvent`

An immutable typed fact emitted after a successful authoritative commit, such as `ActorMoved`, `DoorOpened`, or `DamageApplied`.

### `PresentationEvent`

A projection intended for view, audio, UI, or animation. It may be derived from domain events but is not itself authoritative.

### `RuleQuery`

A pre-resolution request for declarative restrictions, modifiers, replacements, costs, or reasons. C++ rules and GDScript contribute values to an accumulator. They do not mutate the world.

### `RuleContribution`

One immutable contribution to a rule query, with source identity, phase, operation, value, and reason.

### `Reaction`

A response to an immutable event that queues one or more future commands. A reaction is not reentrant mutation.

## World geometry

### `RegionId`

A `ContentId` identifying a logical map region.

### `HexCoord`

Pointy-top axial coordinates `(q, r)` within a layer. Cube `(x, y, z)` coordinates are derived helpers satisfying `x + y + z = 0`.

### `HexCellId`

A stable address combining region, axial coordinate, and layer. Separate cells may share `(q, r)` on different layers.

### `HexDirection`

One of six canonical edge directions. The enum ordering is stable and documented because it participates in serialization and rotation.

### `HexFacing`

One of six authoritative orientations. It is intentionally distinct from a floating-point yaw.

### `HexPose`

An anchor `HexCellId` plus `HexFacing`.

### `Footprint`

A canonical set of axial offsets occupied relative to an anchor. Offsets rotate by `HexFacing`. A single-cell actor uses the same abstraction as a large creature.

### `EdgeKey`

A canonical identity for the boundary between adjacent cells. Directional traversal facts are stored relative to the canonical edge.

### `CellFacts`

Generated or authored facts about a cell, including surface height, terrain, clearance, traversal base cost, tags, and diagnostic evidence.

### `EdgeFacts`

Generated or authored facts about traversal between two cells, including blocking, cost, step or portal behavior, sight blocking, and attached edge entities.

### `CompiledHexMap`

The immutable runtime graph produced from generated bake facts plus manual overrides. Runtime systems never consult editor scene geometry for logical traversal.

## ECS and capabilities

### `Capability`

A cohesive C++ feature slice that owns some components, commands, events, rules, systems, optional FSMs, persistence codecs, Godot exposure, and tests. It is an organizational and ownership contract, not necessarily a runtime base class.

### `Component`

Plain ECS data stored in EnTT.

### `System`

Behavior operating over components during an explicit phase or command resolution.

### `MachineHost`

A C++ owner for one family of Boost.Ext.SML instances. It provides construction, event delivery, tracing, snapshot, restore, and lifecycle management without placing arbitrary behavior in components.

## Scripting and content

### `Definition Resource`

A typed Godot `Resource` describing stable content data, such as an actor, ability, footprint, door, terrain, or encounter definition.

### `Script Module`

A stateless or explicitly state-managed GDScript behavior resource attached to a global, region, entity, encounter, quest, dialogue, or ability scope.

### `Script Scope`

The stable identity and lifetime under which one script module receives events, owns a state bag, and receives a deterministic random stream.

### `Script State Bag`

Versioned persistent key-value state owned by Dross. Script member variables are not durable authoritative state.

### `ScriptContext`

The capability-scoped API passed to GDScript. It provides queries, deferred commands, state access, deterministic random access, and metadata. It does not provide direct mutation.

## Determinism and persistence

### `MasterSeed`

The seed from which authoritative named random streams are derived.

### `RandomStreamId`

A stable namespaced identifier for one deterministic stream.

### `ReplayLog`

The initial snapshot reference, master seed, content manifest, tick-stamped accepted input commands, and expected checkpoint hashes needed to reproduce a run.

### `CanonicalSnapshot`

A stable ordering and encoding of authoritative state used for save data and deterministic hashing.

### `Quiescent Boundary`

A point after a complete simulation phase where no command is mid-commit, no event callback is active, no follow-up queue is partially drained, and all machine state is snapshot-safe.
