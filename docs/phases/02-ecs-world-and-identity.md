# Phase 02: ECS World and Stable Identity

## Goal

Create the authoritative EnTT world, stable identity index, deterministic entity allocation, spawn and destruction plans, and world read/write boundaries.

## Read first

- `docs/04-capability-architecture.md`
- `docs/05-ecs-world-model.md`
- ADR-0002, ADR-0012, ADR-0016

## Live production consumer

`dross_headless scenario identity-lifecycle` creates a world, spawns named and runtime entities, queries them, destroys one, and emits a deterministic canonical summary.

## Scope

Implement:

```text
WorldConfig and WorldInstanceId
WorldStorage
EntityIndex
EntityIdAllocator
EntityRef
PersistentIdentity component
spawn plan and destroy operation
WorldRead and narrow WorldWrite access
stable iteration helper
identity capability installation
```

Do not implement the full command or event pipeline yet. Spawn and destruction are direct world-construction operations in this phase and will receive command integration later.

## Identity allocation

- world lineage is provided in `WorldConfig` or derived deterministically from authored world identity;
- runtime sequence begins from a defined value and advances monotonically;
- author-placed identity accepts a stored stable sequence or editor identity mapping;
- duplicate IDs and aliases fail before entity visibility;
- destroyed IDs are not reused;
- allocator state is snapshot-ready even though save format comes later.

## World boundaries

`WorldStorage` owns the registry and indexes. Provide only the EnTT access needed by the identity capability and test fixtures.

Do not build repository classes per component. Do not expose `registry()` publicly.

A narrowly scoped internal adapter may offer:

```cpp
registry_view_for<Components...>()
get_component<T>(EntityRef)
mutate_owned<T>(CapabilityToken, EntityRef)
```

The exact design should remain simple. Compile-time friendship or internal headers are acceptable if they preserve the external boundary.

## Spawn plan

A spawn operation validates all foundational identity information before creating an observable entity. If EnTT creation occurs before a later operation, the entity remains private and is destroyed on construction failure.

The phase needs only `PersistentIdentity`, but the shape must permit later capability initializers without a generic variant bag.

## Destroy lifecycle

Destroy removes:

- EnTT entity;
- stable ID mapping;
- alias mapping;
- any identity-owned allocation tracking required.

Later capability cleanup hooks are documented but not implemented as empty registries in this phase.

## Tests first

- deterministic runtime allocation sequence;
- duplicate ID rejection;
- duplicate alias rejection;
- lookup by ID and alias;
- invalid world instance reference rejection;
- stale EntityRef after destruction;
- destroyed ID not reused;
- equivalent worlds created in different storage order produce the same stable identity summary;
- stable iteration sorts by EntityId only when requested;
- no partial entity remains after spawn failure;
- no raw EnTT handle leaks from public headers.

Add a compile or source architecture test for the last point.

## Prohibited shortcuts

- random UUID generation;
- alias as primary identity;
- public `entt::registry&` getter;
- `entt::entity` in save-facing or Godot-facing types;
- string-keyed component storage;
- implicit entity destruction from destructor of a future view object;
- empty lifecycle callback registries with no consumer.

## Validation

```bash
cmake --build --preset linux-debug
ctest --preset linux-debug --output-on-failure
dross_headless scenario identity-lifecycle
```

Run ASan and UBSan. Inspect public headers for EnTT leakage.

## Suggested commits

1. `test: specify persistent entity identity semantics`
2. `feat: add EnTT world storage and identity index`
3. `feat: add deterministic entity lifecycle scenario`

## Exit criteria

- EnTT is the actual storage used by the scenario;
- identity survives storage-handle churn in tests;
- public APIs use EntityRef and EntityId;
- spawn rejection is atomic;
- destruction invalidates references;
- no unused lifecycle abstraction exists.

## Stop conditions

- EnTT component or registry behavior prevents stable identity cleanup as designed;
- EntityId representation cannot encode authored and runtime identity without collision;
- maintaining world instance validation would require Godot object ownership.
