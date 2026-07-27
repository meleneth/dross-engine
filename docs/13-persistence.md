# Persistence

## Goal

Save and load are architectural contracts in the first vertical slice. Dross does not postpone them until ambient runtime state has become impossible to separate.

## Save boundary

A save occurs only at a quiescent boundary:

- no command is mid-validation or commit;
- no event queue is partially draining;
- no script callback is active;
- all follow-up commands for the completed phase are queued consistently;
- machine hosts can snapshot;
- presentation may still be animating, but presentation state is not authoritative.

The initial game permits saves:

- during exploration between command phases;
- during combat at an actor-turn command boundary;
- after an action has committed, even if a cosmetic animation is finishing, provided presentation can reconstruct from state.

## Save container

A Dross save contains a versioned container header and canonical sections.

```text
magic and container version
engine build identity
simulation schema version
tick rate and current tick
content and mod manifest
world lineage and instance metadata
master seed and random stream snapshots
compiled map references and hashes
entity records
machine records
script state records
pending external command metadata allowed by policy
canonical state hash
```

The first codec may use a readable structured format through an established serialization library, but the engine API targets DTOs and archive interfaces rather than exposing JSON objects. Compression or a binary codec can be added without changing component ownership.

## Explicit DTOs

Each persistent component defines a stable DTO separate from its in-memory representation.

```cpp
struct DoorStateV1 {
    EntityId entity;
    DoorStateId state;
    std::vector<EdgeKey> edges;
};
```

Encoding and decoding are explicit. A component does not gain persistence through raw memory reflection.

Benefits:

- schema evolution;
- validation before world mutation;
- canonical ordering;
- cross-platform encoding;
- no padding or pointer leakage;
- clear ownership.

## Component codec registry

A generated or explicit registry maps stable component type IDs to codecs.

Each codec supplies:

```text
stable type ID
current schema version
encode from world state to DTO
validate DTO
migrate older DTO versions
decode into a load plan
apply load plan during world construction
canonical hash contribution
```

Registration occurs at the composition root. Duplicate IDs fail engine construction.

## Two-phase load

Loading is not mutation of a live world record by record.

### Phase 1: parse and validate

- parse container and versions;
- verify required content and mods;
- verify map identity;
- migrate DTOs in memory;
- validate references, IDs, and component combinations;
- construct entity, component, machine, script, and random restore plans;
- reject the entire save on required-data failure.

### Phase 2: construct

- create a fresh world instance;
- restore entities and stable indexes;
- apply component plans in dependency order;
- rebuild derived indexes;
- restore machine hosts;
- restore script state and random streams;
- validate world invariants;
- publish a typed `WorldLoaded` fact and presentation snapshot;
- swap the new world into the host only after success.

The old world remains intact if validation fails.

## References

Save DTOs use stable `EntityId`, `ContentId`, `RegionId`, `HexCellId`, and `EdgeKey`. They do not store live `EntityRef`, `entt::entity`, `ObjectID`, node paths, or resource pointers.

On load, references are resolved after all entity identities exist. Missing required references are errors. Optional references are explicitly typed as optional.

## Machine persistence

Every persistent SML machine stores its explicit snapshot DTO. Restore creates a new machine and drives the supported restore path. The save does not serialize SML implementation internals.

## Script persistence

The save contains:

```text
script module ID
script scope identity
state schema version
canonical key-value state
random child-stream snapshot if independently advanced
```

GDScript object member fields are excluded.

A script module provides migrations for its state schema through the Dross script migration API. Missing required module or migration fails load.

## Random persistence

Save the master seed, random algorithm version, and current state of every created authoritative stream. A stream that has never been created need not have a record because its initial state is derivable.

## Content and mod manifest

A save records exact package IDs, versions, dependency resolution order, and content hashes. Initial policy:

- missing required package fails load;
- changed package hash produces a compatibility error unless a declared migration accepts it;
- additional unrelated packages may be allowed only when deterministic load order and override rules prove they do not alter required content;
- users may explicitly invoke a developer override, but the resulting save is marked migrated or tainted rather than silently accepted.

## Canonical ordering

Save output is deterministic:

- maps by region ID;
- entities by `EntityId`;
- components by stable component type ID;
- machine records by family and scope;
- script scopes and keys by stable order;
- maps and sets explicitly sorted before encoding.

Saving the same quiescent state twice produces byte-identical uncompressed canonical payloads.

## Migrations

Migrations are small, ordered, pure functions from one DTO version to the next.

Rules:

- never mutate a committed fixture;
- keep fixture saves for each released schema;
- test each step and full chain;
- preserve unknown optional extension data only when the format explicitly supports it;
- fail rather than guess when required semantics changed;
- document user-visible consequences.

## Save errors

Use typed error categories:

```text
container format
unsupported engine schema
missing content or mod
content hash mismatch
migration missing or failed
invalid entity reference
invalid component combination
machine restore failure
script state failure
random restore failure
world invariant failure
I/O failure
```

Errors contain paths and identities but do not expose raw implementation pointers.

## First-slice persistence acceptance

A save and reload must preserve:

- player and field mouse identity;
- alive or dead state and hit points;
- logical pose and facing;
- movement and combat machine state;
- door open, closed, or locked state;
- AP and turn order;
- script module bindings and state bag;
- current tick;
- RNG streams;
- content manifest;
- compiled map identity;
- replay compatibility.

A headless uninterrupted scenario and a save-reload-resume scenario must finish with the same canonical state hash.
