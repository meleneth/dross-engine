# Phase 09: Typed GDScript Runtime

## Goal

Expose typed query, command, rule, event, state, and deterministic random APIs to GDScript through one real entity script and one real region script.

## Read first

- `docs/10-gdscript-scripting-runtime.md`
- ADR-0009, ADR-0019

## Live production consumer

A headless Godot scenario installs:

- one region script that requests a mode transition through a typed command when a tested condition is met;
- one entity script that contributes to the existing placement or interaction rule, reacts to a typed event, writes persistent script state through deferred semantics, and uses its deterministic random stream.

The same core scenario runs with a fake C++ script port and the real Godot port.

## Scope

Implement only the script scopes used by the scenario:

```text
DrossScriptModuleDefinition Resource
DrossEntityScript base Resource or RefCounted contract
DrossRegionScript base contract
DrossScriptContext
DrossQueryApi subset
DrossCommandApi subset
DrossScriptStateApi
DrossRandomApi
one generated typed event wrapper
one generated typed rule query wrapper
GodotScriptRuntime implementing ScriptRuntimePort
script installation, cached callback discovery, ordering, and fault handling
script state persistence codec
```

Do not implement empty quest, dialogue, global, or ability script classes yet. Their names remain documented only.

## Stateless behavior resource

Script module Resources are treated as immutable behavior definitions after load. Durable mutable state belongs to Dross state bags. Callback-local values are ordinary locals.

Test that a mutable GDScript member does not survive reload and is not used by the demo behavior.

## Callback transaction

For each callback, buffer:

- rule contributions;
- commands;
- script state writes;
- trace annotations.

If the callback faults, discard its buffered output. Apply the documented world-fault policy according to whether commit already occurred.

## Generated wrappers

Extend the phase 04 generator to produce:

- immutable Godot event class;
- typed getters;
- ClassDB registration;
- rule query wrapper operations;
- GDScript API docs;
- compile and headless tests.

No dictionary fallback for the stable event.

## Deterministic ordering

Install multiple test modules in shuffled resource order and prove callback execution follows region, entity ID, then module ID policy.

## Tests first

- script module ContentId and scope validation;
- cached callback discovery;
- typed event object immutability;
- typed query result;
- command submission deferred to later command phase;
- rule contribution ordered with native phases;
- state write is deferred and persisted;
- deterministic script RNG golden behavior;
- direct invalid EntityRef rejected in C++;
- callback fault before commit rejects command and applies no buffered output;
- event callback fault after commit faults world at safe boundary;
- callback ordering independent from Resource load order;
- generated code idempotence;
- source scan for forbidden direct RNG in authoritative demo scripts.

## Prohibited shortcuts

- `Dictionary` event or command payloads;
- script calling a synchronous `set_component` method;
- arbitrary script fields serialized;
- `has_method` on every event rather than cached installation;
- numeric callback priorities;
- continuing silently after callback fault;
- treating narrow APIs as a hostile-code sandbox;
- implementing unused script scope classes.

## Validation

```bash
./bin/dross generate all
./bin/dross check generated
pytest tools/dross_cli/tests
cmake --build --preset linux-debug
ctest --preset linux-debug --output-on-failure
godot --headless --path godot -s res://tests/run_phase09.gd
```

Record and replay the script scenario twice with the same seed.

## Suggested commits

1. `test: specify typed script context and ordering`
2. `feat: add entity and region script runtime ports`
3. `tool: generate typed Godot event and rule wrappers`
4. `test: prove deferred script state commands and faults`
5. `feat: persist and replay GDScript-authored behavior`

## Exit criteria

- real GDScript changes game behavior only through typed ports;
- script state survives save and reload;
- script random behavior replays;
- callback ordering is stable;
- no direct mutation surface exists;
- only live entity and region script scopes are implemented.

## Stop conditions

- custom virtual or callback behavior cannot be invoked safely through godot-cpp baseline;
- Godot Resource caching prevents immutable module semantics without a different design;
- callback errors cannot be detected and contained before buffered outputs apply;
- typed wrapper generation cannot preserve GDScript usability.
