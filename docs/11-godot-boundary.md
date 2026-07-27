# Godot Boundary

## Integration choice

Dross integrates through GDExtension and `godot-cpp`. It does not begin as a Godot source fork or static custom module.

The core remains a static C++ library. The GDExtension shared library links the core and contains all Godot-specific code.

## Version baseline

The initial conservative baseline is:

```text
Godot editor/runtime: 4.7.1 stable
godot-cpp API target: 4.5 stable
minimum extension compatibility: Godot 4.5
```

The earlier godot-cpp target is intentional because GDExtensions targeting an earlier Godot 4 minor are expected to work on later minors, while the stable godot-cpp line avoids beginning on the v10 beta branch.

Any required Godot 4.6 or 4.7 native API that is absent from 4.5 bindings is a stop condition and requires an ADR before upgrading.

## Adapter targets

`dross_godot` owns:

- GDExtension initialization and class registration;
- conversion between Godot and core values;
- typed `Resource` definitions;
- typed command, query, rule, and event wrappers;
- GDScript runtime implementation;
- world hosting and tick accumulation;
- view registry and presentation projection;
- editor-native analyzers and nodes;
- Godot diagnostic output.

It does not own domain rules.

## World host

A `DrossWorldHost` Godot node owns one core `EngineRuntime` instance.

Responsibilities:

- load compiled content and create the world;
- translate frame time into fixed tick requests;
- receive input commands from GDScript or UI;
- dispatch presentation events after core phases;
- expose world status and diagnostics;
- coordinate save, load, replay, and safe shutdown;
- keep the core runtime alive independently from visual entity nodes.

There is no authoritative autoload singleton. A project may use an autoload to locate the host for UI convenience, but the host owns the runtime explicitly.

## Type conversion

Core value types remain ordinary C++. Godot wrappers are adapters.

Examples:

```text
core ContentId        <-> Godot StringName with validation
core EntityRef        <-> DrossEntityRef RefCounted
core HexCellId        <-> DrossHexCellRef RefCounted or generated immutable wrapper
core command result   <-> typed DrossCommandResult
core domain event     <-> typed immutable Dross event object
core definition DTO   <-> Dross Resource properties
```

Do not contaminate core value types with Godot inheritance or `Variant` storage.

## Definition resources

C++ classes deriving Godot `Resource` expose typed properties through `ClassDB`. They are editor-friendly authoring forms.

At world load, each resource validates and compiles into a Godot-free core definition DTO. The core does not retain a `Ref<Resource>`.

Validation errors include resource path, property, content ID, and reason. Invalid content prevents entering `Running`.

## Script bridge port

The core defines a Godot-free `ScriptRuntimePort` describing rule contribution and event reaction. The adapter implements it with GDScript resources and generated wrappers.

Headless tests use:

- `NullScriptRuntime`;
- `FakeScriptRuntime`;
- scripted deterministic fixtures written in C++.

This proves the simulation does not require the Godot VM.

## Entity views

Visible ECS entities may have presentation nodes, but many entities do not.

`DrossViewRegistry` maps stable `EntityId` to optional `DrossEntityView` nodes.

A view:

- holds an immutable entity reference;
- reads presentation snapshots;
- interpolates between logical poses;
- plays animation and effects from presentation events;
- reports animation acknowledgements to presentation gating only;
- may be pooled or destroyed without destroying the entity.

Closed-container items, off-map entities, abstract quests, or unloaded actors need no node.

## Presentation snapshots

At the end of an authoritative tick, the adapter receives a presentation snapshot containing only data required by views, such as:

```text
entity ID
logical from and to poses
transition start and end ticks
visual definition ID
facing
visibility
presentation tags
```

Views interpolate using render time. They never write their interpolated transform back to the core.

## Animation policy

An accepted action commits domain state before animation completes.

The combat or action FSM may enter a presentation-gated state that refuses the next player command until an acknowledgement or timeout. The acknowledgement means `presentation may continue`, not `damage now becomes real`.

Required failure behavior:

- missing animation produces a diagnostic and immediate or fallback acknowledgement;
- animation timeout releases the gate according to policy;
- duplicate acknowledgements are ignored and traced;
- no health, AP, inventory, door, or quest mutation occurs inside animation callbacks.

## Input

Input is translated into typed commands.

Examples:

```text
hovered cell -> query path preview
click destination -> MoveTo command
select ability -> UI state only
click target -> PerformAbility command
click door -> OpenDoor command
end turn -> EndTurn command
```

Selection raycasts may use Godot physics. The result is converted into a stable cell or entity reference and validated again by the core.

## Navigation and physics

Godot navigation and physics are non-authoritative.

Allowed uses:

- editor geometry bake queries;
- selection raycasts;
- visual collision and camera behavior;
- particles, debris, ragdolls, and effects;
- authoring diagnostics.

Forbidden uses:

- combat path cost;
- authoritative occupancy;
- attack range;
- movement completion;
- damage or hit determination;
- line-of-sight truth unless compiled into Dross map data.

## Headless Godot integration

Godot boundary tests run with the headless executable and verify:

- GDExtension loads;
- classes appear in `ClassDB`;
- typed resources instantiate and validate;
- GDScript can call typed command and query methods;
- script callbacks receive typed events;
- no core mutation API is exposed;
- a view follows a presentation snapshot without changing core pose;
- editor plugin scripts parse and load in editor tests when applicable.
