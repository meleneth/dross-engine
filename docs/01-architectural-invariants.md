# Architectural Invariants

These invariants are acceptance criteria, not aspirations.

## Boundary invariants

1. `dross_core` compiles and tests without Godot installed.
2. No source under the core include or source roots includes `godot_cpp`, Godot headers, or Godot-generated bindings.
3. Godot-facing types translate into core value types at the adapter boundary.
4. GDScript never receives `entt::registry`, `entt::entity`, mutable component references, raw native pointers, or mutation callbacks.
5. The authoritative simulation does not read Godot node transforms, animation state, physics results, navigation paths, signal order, or frame delta to decide game outcomes.
6. Presentation may lag or interpolate, but authoritative state remains complete and valid.

## ECS invariants

1. EnTT owns in-memory entity and component storage.
2. Components are data. Systems, command handlers, and machine hosts own behavior.
3. Each mutable component has a capability owner responsible for its invariants.
4. A system may mutate only the components it owns while resolving an accepted command or an explicit internal phase.
5. Cross-capability requests use commands, queries, or immutable events. They do not reach into another capability's private storage.
6. Semantic ordering never depends on EnTT sparse-set order unless that order is explicitly documented, tested, and proven stable for the operation.
7. Persistent identity is separate from `entt::entity` and is never reused within a world lineage.

## Command and event invariants

1. Commands are typed requests that may be accepted or rejected.
2. Events are immutable facts emitted only after authoritative mutation commits.
3. A rejected command leaves the world unchanged and emits a typed rejection trace, not a misleading domain event.
4. Validation and rule collection finish before mutation begins.
5. Expected failures occur before commit. A failure during the no-fail commit portion is an invariant fault.
6. Event reactions cannot reenter the active command handler. They enqueue follow-up commands for a later command phase.
7. Script callbacks cannot mutate the event being observed.
8. Stable event and command types are schema-generated. A `Dictionary` is not an acceptable replacement.
9. Logs and signals do not substitute for domain events.

## FSM invariants

1. Known engine and capability lifecycles use Boost.Ext.SML.
2. Each machine has typed states, typed events, explicit guards, explicit actions, and a tested unexpected-event policy.
3. Capability machines are independent. Dross does not create a universal entity-state enum.
4. Persistent machines provide an explicit snapshot and restore contract.
5. Restoring a machine is tested from every persisted state.
6. Animation states and domain states are distinct. Presentation may have its own machine, but it cannot mutate the domain machine by implication.
7. A runtime author statechart system is deferred and may not be improvised as a switch statement or dictionary-driven pseudo-machine.

## Hex-world invariants

1. Logical world position is represented by Dross hex types, not a `Vector3`.
2. World transforms are derived from logical pose.
3. Cells and edges both contain traversal semantics.
4. A door is an edge-anchored entity, not a boolean smeared onto a cell.
5. Multi-hex footprints and facing are part of path validity from the start.
6. The path state is `HexPose`, not merely `HexCoord`.
7. Overlapping elevation layers are representable even before the first map uses them.
8. The runtime overlay, pathfinder, occupancy rules, and editor all consume the same compiled hex map.
9. Generated bake facts and human overrides are separate assets. Rebaking never silently erases manual intent.

## Determinism invariants

1. Authoritative simulation advances in integer ticks.
2. `RandomHub` and `pcg64` are the only authoritative random source.
3. Standard-library random distributions, `std::shuffle`, `std::hash`, Godot random functions, and wall-clock time are forbidden in authoritative logic.
4. Random streams are named and derived through a stable algorithm.
5. Ordering that changes outcomes uses explicit phases and stable keys.
6. Authoritative numeric decisions use integers, fixed-point values, or explicitly specified rational arithmetic. Presentation floats do not flow backward into simulation.
7. Given the same engine version, content manifest, initial snapshot, master seed, and tick-stamped command stream, supported platforms produce the same accepted commands, events, and canonical state hashes.
8. Replay records commands and metadata, not random outcomes.

## Persistence invariants

1. Saves contain explicit, versioned DTOs and component records.
2. Raw registry snapshots are forbidden.
3. Godot nodes, resource caches, script object fields, callables, native addresses, event listener instances, and active stacks are not serialized.
4. Script durable state uses the Dross state bag with supported types and schema versions.
5. Saves occur only at a declared quiescent boundary.
6. Unknown required records or missing mods fail loudly. Data is never silently discarded.
7. Every migration is tested against a committed fixture.
8. Save and replay headers include the engine schema version and content or mod manifest identity.

## Quality invariants

1. Behavior begins with a failing test.
2. Dependencies are pinned, marked as system dependencies, and do not weaken project warning policy.
3. Project code is warning-clean on supported compilers.
4. Sanitizer jobs exist before code becomes difficult to isolate.
5. Architecture checks prevent Godot dependencies from leaking into core.
6. Generated files are deterministic and checked into version control when they are part of the public API.
7. Running the generator twice without schema changes produces no diff.
8. No phase concludes with unused buses, uncalled handlers, orphan event types, or placeholder production code.
