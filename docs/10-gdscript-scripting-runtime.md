# GDScript Scripting Runtime

## Goal

Editing the game should mostly mean creating typed resources, placing scenes, and writing GDScript against a stable Dross API. Adding a new fundamental storage or rule capability should mostly mean C++.

The scripting boundary is powerful, but it is not a back door around invariants.

## Data, behavior, and presentation split

### Typed definition resources

C++ GDExtension classes define stable Inspector-editable data shapes:

```text
DrossActorDefinition
DrossAbilityDefinition
DrossFootprintDefinition
DrossDoorDefinition
DrossTerrainDefinition
DrossFactionDefinition
DrossEncounterDefinition
DrossScriptModuleDefinition
DrossHexBakeProfile
```

Authors create `.tres` instances. Definitions are data and references, not mutable world state.

### GDScript behavior modules

GDScript supplies authored behavior in scopes:

```text
GlobalScript
RegionScript
EntityScript
EncounterScript
QuestScript
DialogueScript
AbilityScript
```

The first slice needs only region and entity script behavior. Other scopes are named now so identity and ordering do not need reinvention, but no unused runtime class should be implemented before a production consumer exists.

### Godot scenes

Scenes own visual prefabs, models, skeletons, animation graphs, audio, lights, cameras, effects, UI, and editor placement handles.

A scene node is not the authoritative entity merely because it displays one.

## Script module definition

A script module is represented by a typed resource containing:

```text
module ContentId
scope kind
GDScript Script resource
subscriptions or generated callback discovery metadata
schema version for state
exported author configuration
```

The runtime treats the definition as immutable after world load.

## Script state

Durable state lives in a Dross-owned `ScriptStateBag` keyed by script scope.

Supported initial types:

```text
bool
signed and unsigned fixed-width integer
ContentId
EntityId
HexCellId
small arrays of supported values
small maps with stable string or ContentId keys
```

Floating-point values, Godot objects, Nodes, Resources, callables, arbitrary Variants, and native pointers are not valid durable script state.

GDScript member fields are ephemeral implementation details. They must not be required for authoritative behavior across save, reload, replay, or hot reset.

State mutation uses deferred command semantics. An ergonomic `ctx.state.set_bool(...)` call appends a typed script-state command; it does not mutate the state bag reentrantly.

## Script context

Callbacks receive a capability-scoped context with narrow APIs:

```text
ctx.owner       current entity or scope identity when applicable
ctx.query       read-only typed world queries
ctx.commands    deferred typed command submission
ctx.state       typed persistent state query and deferred update API
ctx.random      deterministic scope stream
ctx.trace       structured developer annotations, not domain events
ctx.tick        current authoritative tick
```

There is no `ctx.registry`, `ctx.world.set`, or generic component dictionary.

## Typed events

Stable domain events are exposed as generated immutable GDExtension `RefCounted` wrappers.

Example:

```gdscript
func on_actor_killed(
        event: DrossActorKilledEvent,
        ctx: DrossScriptContext) -> void:
    if event.victim == ctx.owner:
        ctx.state.set_bool(&"observed_death", true)
```

The runtime discovers and caches generated callback names once during script installation. Dynamic `has_method` checks do not occur for every event dispatch.

A generic development-only inspection event may exist for debugging, but production behavior uses typed callbacks.

## Rule contribution

Generated rule query wrappers expose only meaningful operations.

```gdscript
func contribute_attack_rules(
        query: DrossAttackRuleQuery,
        ctx: DrossScriptContext) -> void:
    if ctx.query.has_trait(query.target, &"dross:sacred_mouse"):
        query.add_damage_multiplier(0, 1, &"dross:sacred_mouse")
        query.add_reason(&"dross:divine_intervention")
```

A script may reject, modify, or replace according to the rule family's declared algebra. It cannot directly apply damage or change AP during rule contribution.

## Event reaction

```gdscript
func on_damage_applied(
        event: DrossDamageAppliedEvent,
        ctx: DrossScriptContext) -> void:
    if event.target == ctx.owner:
        ctx.commands.show_bark(ctx.owner, &"dross:mouse_squeak")
```

The queued command executes in a later command phase. The callback is not allowed to call into a synchronous mutation API.

## Deterministic ordering

Script callbacks run in the locked phase order:

1. region scope;
2. entity scopes in stable `EntityId` order;
3. encounter or quest scopes in stable module ID order;
4. global scopes in stable module ID order.

Within one scope, modules sort by namespaced module ID. Dependencies may constrain load order, but numeric callback priorities are not part of the initial API.

## Script random access

Authoritative GDScript must use `ctx.random`. The stream is derived from:

```text
master seed
script module ID
scope identity
random stream purpose
```

The adapter exposes deterministic integer and rational operations. Direct use of Godot random functions in authoritative scripts is prohibited by documentation, examples, and a source scan in project tests.

## Fault behavior

- a rule callback fault rejects the command before mutation;
- an event-reaction fault marks the world faulted after the safe event boundary;
- callback output is transactional, so a failing callback contributes no partial set of commands or state writes;
- errors include module, scope, callback, tick, causation, and stack information;
- release behavior does not silently continue with a half-applied authored rule.

## Hot reload

Initial hot-reload policy:

- scripts and definition resources may be reloaded in editor workflows;
- an active authoritative world is reset or fully reloaded at a safe boundary;
- Dross does not patch live script objects, machine definitions, and persistent state in place;
- the editor displays why a reset is required.

## Trusted mods

GDScript mods are trusted executable code. Capability-scoped APIs provide architecture and compatibility, not a security sandbox.

## What requires C++

Authors or engine forks must add C++ for:

- a new ECS component type;
- a new authoritative storage shape;
- a new command or event family;
- a new rule algebra or effect primitive;
- a new deterministic capability system;
- a new compile-time machine family;
- a new save component codec;
- native editor analysis that must exactly share core math.

The Python scaffolder supplies copyable examples and registration work so this path is explicit rather than mysterious.

## What should remain GDScript

Use GDScript for:

- special encounter conditions;
- map and entity reactions;
- quest and dialogue behavior built from existing commands;
- conditional ability behavior using existing rule contributions;
- barks and cutscene choreography;
- editor controls and author workflow;
- view-local presentation logic.
