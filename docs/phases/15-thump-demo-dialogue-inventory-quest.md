# Phase 15: ThumpDemo Dialogue, Inventory, and Quest Slice

## Status

**Active.** ADR-0025 is accepted. Implementation begins with the Dross versus
ThumpDemo project and namespace boundary before adding new capabilities.

## Goal

Prove how a small game is organized on top of Dross by extending ThumpDemo with
one complete dialogue, inventory, and quest loop while keeping all
game-specific prose and behavior outside generic engine infrastructure.

## User-visible result

The player can:

1. make contact with the field mouse;
2. automatically receive the caretaker's request through the game-owned FSM;
3. inspect the active quest and its authored stage;
4. defeat the mouse with Thump;
5. receive one `thump_demo:mouse_tail` inventory item through a scripted event
   reaction;
6. speak with the caretaker again;
7. see a dialogue option gated by inventory and quest queries;
8. hand over the item;
9. observe the item removal and quest completion;
10. save, reload, record, and replay the complete sequence.

## Project organization

Game-owned content moves under:

```text
godot/thump_demo/
├── scenes/
├── content/
├── scripts/
└── ui/
```

All game-owned IDs use `thump_demo:`. Generic Dross GDScript bases and APIs move
under `godot/dross/`. Phase-only fault fixtures and old boundary examples move
under `godot/tests/fixtures/`.

No compatibility shim retains the mixed `demo:` or `dross_demo:` namespace in
the production ThumpDemo path. Committed replay/save fixtures receive explicit
migration or replacement rather than silent reinterpretation.

## Native capability scope

### Inventory

- canonical item counts per authoritative entity;
- typed `GrantItem` and `RemoveItem` commands;
- immutable `ItemGranted` and `ItemRemoved` events;
- side-effect-free rejection for invalid identity, item, count, or insufficient
  quantity;
- read-only count and possession queries;
- explicit component codec and canonical hash contribution.

### Dialogue

- one typed active session between stable entity references;
- `BeginDialogue`, `ChooseDialogueOption`, and `EndDialogue` commands;
- immutable session and choice events;
- a deterministic option-contribution query exposed to GDScript;
- rejection of choices that were not offered for the current session;
- session lifecycle snapshot and restore.

Dialogue presentation text is authored content. Dross stores stable dialogue
and option IDs required for validation and replay, not localized prose.

### Quest progress

- progress keyed by stable quest ID;
- inactive, active, completed, and failed lifecycle states;
- stable authored stage ID while active;
- typed start, advance, complete, and fail commands;
- immutable progress events;
- read-only status and stage queries;
- lifecycle snapshot, restore, codec, and canonical hash contribution.

Phase 15 implements only the transitions consumed by ThumpDemo. Repeatable
quests, branching graph interpretation, objective DSLs, and general runtime
statechart authoring remain deferred.

### ThumpDemo caretaker FSM

`src/thump_demo/fsm/caretaker_machine.*` owns the compile-time
`thump_demo::CaretakerMachine`. Its states are `waiting_for_mouse_contact`,
`hunt_assigned`, `waiting_for_tail`, and `settled`. Committed mouse contact and
quest events drive its SML transitions. The generic `QuestRuntime` remains the
authority for quest facts; the caretaker machine is game-owned orchestration
and reconstructs from those facts during load.

## GDScript examples

The phase must leave three short, production-used examples:

- `caretaker_dialogue.gd` reacts to typed quest lifecycle events, contributes
  options from inventory and quest queries, then submits deferred typed hand-in
  commands;
- `field_mouse.gd` reacts to committed death and grants the authored item;
- `mouse_quest.gd` reacts to typed dialogue/inventory facts and records any
  script-local durable presentation state through `ctx.state`.

Examples use typed callback parameters and typed API methods. They do not use
generic dictionaries, direct ECS access, Godot randomness, or GDScript member
fields as durable state.

## Tests

Write native behavior tests first for:

- inventory grant, remove, rejection, ordering, snapshot, and restore;
- every dialogue and quest SML transition and unexpected-event policy;
- every ThumpDemo caretaker transition, rejection, snapshot, and restore path;
- canonical dialogue option ordering;
- unoffered or stale dialogue choice rejection with no state change;
- quest transition rejection with no state change;
- save/load and replay across pre-quest, active quest, item-held, and completed
  boundaries;
- canonical hashes independent of insertion and Resource load order.

Add headless and Godot tests for the complete ThumpDemo sequence, typed GDScript
callbacks, UI projection, view reconstruction, invalid content diagnostics, and
frame-cadence independence.

## Documentation

Update the root README with:

- the Dross versus ThumpDemo folder and namespace boundary;
- copyable inventory query and command examples;
- the caretaker dialogue example;
- the quest reaction example;
- how to add game content without adding an engine capability;
- when a new authoritative storage shape still requires C++.

## Validation

Run:

```sh
./bin/dross generate all
./bin/dross check generated
PYTHONPATH=tools/dross_cli/src .venv/bin/python -m pytest -q

cmake --build --preset linux-debug
ctest --preset linux-debug --output-on-failure
cmake --build --preset linux-debug --target tidy

cmake --build --preset linux-asan-ubsan
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=print_stacktrace=1 \
  ctest --preset linux-asan-ubsan --output-on-failure

godot --headless --path godot --script res://tests/run_all.gd
```

Build and smoke-launch the Linux prototype package outside the repository.
Windows and Steam Deck hardware retain the explicit Phase 14 platform
exceptions until those targets become available.

## Exit criteria

- ADR-0025 is accepted;
- the complete loop works headlessly and through Godot;
- every introduced command, event, query, machine, codec, binding, and script
  callback has a live ThumpDemo consumer;
- save and replay match at every required boundary;
- Dross core contains no ThumpDemo content or prose;
- generated output and the working tree are clean;
- all locally available validation passes.

## Stop conditions

- dialogue option validation requires trusting UI state;
- quest behavior requires a generic runtime-authored FSM language;
- GDScript must directly mutate inventory or quest storage;
- save migration would reinterpret an existing stable component ID;
- the three capabilities cannot be composed without a generic effect
  dictionary or synchronous callback mutation.
