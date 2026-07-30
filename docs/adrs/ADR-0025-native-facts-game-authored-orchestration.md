# ADR-0025: Native Facts and Game-Authored Orchestration

Status: Accepted

## Context

The first post-foundation slice needs dialogue, inventory, and quest behavior.
It must also demonstrate how a real game is arranged on top of Dross rather than
placing game-specific code beside generic engine infrastructure.

Inventory ownership, active dialogue sessions, offered choices, and quest
progress affect command legality, save data, replay, and canonical hashes.
They are authoritative facts and cannot live only in Godot nodes or GDScript
member variables.

Dialogue prose, quest-specific conditions, rewards, and reactions vary by game.
Making those details native Dross concepts would couple the engine to
ThumpDemo. Implementing a generic runtime-authored state-machine language would
prematurely resolve a broader deferred decision.

## Decision

Dross owns three narrow typed capabilities:

- inventory stores canonical item counts for an entity;
- dialogue validates one active session and accepts only a currently offered
  typed option;
- quest progress stores a typed lifecycle and stable authored stage ID.

Each capability owns its C++ state, typed commands, immutable events, queries,
save codec, replay/hash contribution, and narrow Godot binding. Distinct
dialogue and quest lifecycle states use Boost.Ext.SML.

GDScript owns game-specific orchestration through typed ports:

- dialogue modules contribute declarative option IDs and presentation text;
- option callbacks submit deferred inventory, dialogue, or quest commands;
- quest and entity modules react to immutable events;
- authored conditions use read-only inventory and quest queries;
- durable script-local values use the existing Dross script-state API.

Dross does not interpret dialogue prose, execute arbitrary effect dictionaries,
create runtime component types, or introduce a general authored FSM language.

The proving game is named **ThumpDemo**. Its content IDs use the
`thump_demo:` namespace. Its scenes, resources, and behavior scripts live under
`godot/thump_demo/`. Generic GDScript bases and engine adapter code remain under
a Dross-owned path. Test-only fixtures remain under test-owned paths.

## Consequences

- Games receive copyable GDScript examples without gaining direct mutation
  access.
- Save/replay behavior remains native, typed, and deterministic.
- Dialogue wording and quest-specific sequencing can change without changing
  Dross C++.
- The slice introduces several small capability APIs, but every one has a live
  ThumpDemo consumer in the same phase.
- Branching quest graphs, localization pipelines, general effect languages,
  and runtime-authored FSM editors remain deferred.

## Enforcement

- no `Dictionary` payload for stable inventory, dialogue, or quest APIs;
- no game-specific prose or `thump_demo:` ID in `dross_core`;
- no authoritative inventory or quest state in Godot nodes or script members;
- rejected commands leave all three capability states unchanged;
- offered dialogue options have canonical ordering independent of Resource load
  order;
- every state value round-trips through save and replay;
- architecture tests enforce the Dross/ThumpDemo source boundary.
