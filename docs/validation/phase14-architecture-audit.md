# Phase 14 Architecture Audit

Recorded 2026-07-29 through commit `ee4534c`.

## Result

Pass for the locally available source and Linux validation surface. No live
boundary violation, duplicate authoritative model, forbidden nondeterministic
API, or placeholder production path was found.

A later dependency-registration pass found four packages with no live include
or link consumer: nlohmann/json, fmt, spdlog, and RapidCheck. Their active CPM
registrations and lock/notice entries were removed; their possible future roles
remain documentation-only until a production or test consumer is introduced.

Windows and Steam Deck device behavior are outside this source audit and remain
unverified as recorded in the Phase 14 validation matrix.

## Automated checks

The seven architecture tests pass:

```sh
ctest --preset linux-debug -R '^architecture\.' --output-on-failure
```

- `architecture.no_godot_in_core`
- `architecture.no_entt_in_public_api`
- `architecture.no_forbidden_random_api`
- `architecture.no_forbidden_gdscript_random`
- `architecture.dependency_inventory_current`
- `architecture.source_registration_current`
- `architecture.package_configuration_current`

The source-registration check covers implementation files in both directions
and rejects private adapter or tool headers without an include consumer.
The package-configuration check locks the production resource filters and
platform GDExtension library mappings for both prototype targets.

The configured clang-tidy target also passes all 34 project translation units
with project warnings treated as errors:

```sh
cmake --build --preset linux-debug --target tidy
```

## Boundary review

| Audit question | Evidence and result |
| --- | --- |
| Godot types in core | None. The architecture test passes; source search finds Godot types only in `src/godot` and Godot API generator templates. |
| EnTT ownership | EnTT registry access and component mutation are confined to `src/core/world/world_storage.cpp`; public headers do not expose EnTT. |
| Event transport | The command/event kernel owns an eventpp queue, emits immutable placement facts, and has production handlers plus focused listener-order tests. |
| Lifecycle authority | World, simulation mode, movement, combat, and door lifecycles use Boost.Ext.SML production machines. |
| Duplicate authoritative models | Movement, occupancy, identity, door, combat, and script durable state each have one core owner. Godot objects consume snapshots or submit requests. |
| Godot/GDScript authority | Adapter properties describe authored content or presentation. GDScript receives typed context objects and cannot access EnTT storage or component mutation. |
| Deterministic iteration and randomness | No `unordered_map`, `unordered_set`, `std::hash`, standard random engine, `rand`, or wall-clock API occurs in authoritative core or headless scenario code. |
| Save and replay coverage | Save components, runtime boundaries, script state, command metadata, RNG streams, machine state, events, and canonical section details have round-trip or divergence tests. |
| Dead or speculative abstractions | Full builds, tests, and clang-tidy expose no unused project declarations or registrations. Reviewed buses, codecs, snapshots, and generated bindings have live consumers. |
| Placeholder production paths | No `TODO`, `FIXME`, `HACK`, placeholder, or “not implemented” marker occurs in project production source. |
| Generated drift | `generate all` and `check generated` pass and leave the tree unchanged, as recorded in the validation matrix. |

Third-party `godot/addons/gdUnit4` implementation comments are excluded from
the project placeholder audit. They are vendored dependency internals, not
Dross production paths.

## Targeted source searches

The audit used targeted searches equivalent to:

```sh
rg '#include\s*[<"]godot|godot::|Variant|Dictionary|Node\b' \
  include/dross src/core tools tests/unit
rg 'unordered_(map|set)|std::hash|random_device|mt19937|rand\s*\(|srand\s*\(' \
  include/dross src/core tools/dross_cli
rg 'TODO|FIXME|HACK|placeholder|not implemented' \
  include/dross src/core src/godot tools/dross_cli generated/include generated/src
rg 'registry\.|emplace<|replace<|patch<|remove<' \
  src/core src/godot tools/dross_cli include/dross
```

Matches were inspected rather than treated as failures by string alone.
Generator templates legitimately contain Godot adapter types, and diagnostic
text containing words such as “pending” is not placeholder architecture.
