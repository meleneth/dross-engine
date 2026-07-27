# ECS World Model

## EnTT role

EnTT is the authoritative in-memory storage and query mechanism. Dross uses it deliberately rather than wrapping every operation until EnTT becomes an invisible imitation.

The engine still provides a narrow `WorldStorage` owner so identity, lifetime, persistence, and capability boundaries are controlled.

```cpp
class WorldStorage {
public:
    EntityRef create_entity(const SpawnIdentity& identity);
    Result<void, DestroyError> destroy_entity(EntityRef entity);
    bool valid(EntityRef entity) const;

private:
    entt::registry registry_;
    EntityIndex entity_index_;
    WorldInstanceId instance_id_;
};
```

The raw registry remains private to core systems and capability installation. It is never exposed to Godot or GDScript.

## Identity model

`entt::entity` is an ephemeral storage handle. `EntityId` is persistent identity.

Recommended initial representation:

```cpp
struct EntityId {
    std::uint64_t lineage;
    std::uint64_t sequence;
    auto operator<=>(const EntityId&) const = default;
};
```

- `lineage` identifies the world or authored identity lineage.
- `sequence` is allocated monotonically and deterministically.
- authored entities may receive stable IDs derived by the content compiler from a stored editor identity, not from a random UUID generated at every bake;
- runtime spawned entities use the world's deterministic sequence allocator;
- destroyed IDs are never reused.

`EntityIndex` maps stable IDs to current EnTT handles and optional aliases to stable IDs.

## Reference validation

`EntityRef` contains:

- `WorldInstanceId`;
- `EntityId`.

Lookup verifies both. A reference from a previous load is invalid even if an entity with the same persistent ID exists in the new instance. Save DTOs store `EntityId`, not live `EntityRef` objects.

GDScript-facing `DrossEntityRef` is an immutable `RefCounted` wrapper around these values plus a weak connection to the world host for validity checks. Every command validates it again in C++.

## Component style

Components are small, explicit data records. Examples for the first slice:

```cpp
struct PersistentIdentity {
    EntityId id;
    std::optional<EntityAlias> alias;
};

struct HexPoseComponent {
    HexPose pose;
};

struct FootprintComponent {
    FootprintId footprint;
};

struct MovementStateComponent {
    MovementStateSnapshot state;
};

struct DoorComponent {
    DoorDefinitionId definition;
};

struct CombatantComponent {
    Initiative initiative;
    ActionPoints current_ap;
    ActionPoints maximum_ap;
};

struct HealthComponent {
    HitPoints current;
    HitPoints maximum;
};
```

Use strong value types for quantities that should not be mixed. Do not create one `Stats` dictionary.

## Ownership

The first-slice ownership map:

| Component | Owner |
|---|---|
| `PersistentIdentity` | identity capability |
| `HexPoseComponent` | movement capability |
| `FootprintComponent` | hex-world capability |
| `MovementStateComponent` | movement capability |
| `DoorComponent` | door capability |
| `DoorStateComponent` | door capability |
| `CombatantComponent` | combat capability |
| `HealthComponent` | health or combat capability, selected once and documented |
| `ScriptBindingsComponent` | scripting capability |
| `DefinitionRefsComponent` | content capability |

A component has one mutation owner even when many systems read it.

## World views

Core code distinguishes read and write authority through dependency shape, not a fake second ECS.

- `WorldRead` exposes typed read-only queries and stable iteration helpers.
- `WorldWrite` is available only inside command commit or explicit engine phases.
- capability internals may receive a narrow registry adapter when EnTT views are necessary.
- tests can build worlds through public spawn or fixture builders rather than reaching into private registry internals.

Do not create a general repository object per component. EnTT is already the storage library.

## Stable iteration

Many ECS operations are mathematically order-independent. Those may use direct EnTT views.

When order changes outcomes, use one of:

- explicit initiative order;
- stable `EntityId` sort;
- stable content ID order;
- explicit phase registration order;
- a capability-owned ordered index.

Never depend on pointer values, insertion order that is not part of the contract, or unordered container iteration.

Provide helpers such as:

```cpp
template<class... Components>
std::vector<EntityRef> stable_entities_with(const WorldRead& world);
```

The helper sorts only when semantic order is needed. Do not impose sorting on every ECS loop.

## Entity creation

Entity creation is a command or content-load operation that produces a complete valid entity.

Use an explicit spawn plan:

```cpp
struct SpawnPlan {
    SpawnIdentity identity;
    DefinitionRefs definitions;
    HexPose pose;
    FootprintId footprint;
    std::vector<CapabilityInitializer> initializers;
};
```

The exact representation may evolve, but partial publicly visible entities are forbidden. Capability initializers validate before the entity becomes observable.

## Entity destruction

Destruction is a lifecycle operation:

1. validate destruction request;
2. emit any pre-destruction capability commands if the design requires them;
3. remove capability machine instances and external indexes;
4. destroy the EnTT entity;
5. remove stable ID mapping;
6. emit immutable `EntityDestroyed` after commit;
7. invalidate views and script scopes through reactions.

Do not let a Godot node's `queue_free()` destroy the authoritative entity by implication.

## Derived and cached state

Derived indexes are allowed when they have:

- one owner;
- deterministic rebuild behavior;
- an invalidation path driven by domain mutation;
- tests comparing rebuild and incremental results;
- explicit persistence policy, usually rebuild rather than serialize.

Examples include occupancy indexes, alias indexes, initiative order, and script subscription indexes.

## World lifecycle

The world lifecycle is an SML machine with at least:

```text
Empty -> Loading -> Ready -> Running -> Saving -> Running
Running -> Unloading -> Empty
Any operational state -> Faulted
```

The exact transitions arrive in the FSM phase. World storage itself must not silently enter a half-loaded state.
