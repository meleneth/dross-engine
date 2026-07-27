# Phase 12: Turn-Based Combat, Thump, and Optional Door

## Goal

Implement individual-initiative combat, action points, typed ability resolution, the `Thump` attack, an edge-anchored side door, capability-specific SML machines, GDScript reactions, and presentation that cannot alter committed results.

## Read first

- `docs/18-first-vertical-slice.md`
- `docs/07-command-event-pipeline.md`
- `docs/08-fsm-architecture.md`
- ADR-0004, ADR-0008, ADR-0020

## Live production consumer

The player can approach the field mouse without using the door, enter combat, take a turn, move if required, use Thump, and observe the result. The optional side door can be opened and closed separately and changes traversal across its edge.

## Scope

Implement capabilities needed by the slice:

```text
Combatant and Health components
initiative and deterministic turn order
CombatSession and actor-turn SML machines or one justified session machine
RequestCombatStart, EndTurn, PerformAbility commands
AbilityDefinition typed Resource and compiled DTO
Thump definition and core effect primitive
AP costs and spending
attack range and target rule query
seeded hit or damage roll through RandomHub
DamageApplied and ActorKilled events when consumed
field mouse entity script behavior
DoorDefinition Resource and compiled DTO
DoorState component and DoorLifecycle SML machine
EdgeFootprint
OpenDoor and CloseDoor commands
DoorOpened and DoorClosed events when consumed
traversal contribution from door state
presentation events and animation acknowledgements
```

Do not add inventory, dialogue, quests, cover, aimed shots, status effects, or weapon taxonomies.

## Turn policy

Initial combat policy:

- individual actor turns;
- initiative sorted descending or ascending according to a named policy, locked by tests;
- stable EntityId tie breaker;
- AP refresh at turn start;
- one active actor;
- explicit EndTurn;
- dead or removed actors skipped;
- combat ends when the configured encounter condition is met.

Turn ordering is a replaceable C++ policy, but implement only this one.

## Thump

Thump is content data using existing capability primitives:

```text
ContentId: dross-demo:thump
range: adjacent legal target
AP cost: explicit integer
target: living actor
effect: deterministic or seeded physical damage
presentation cue: dross-demo:thump
```

The handler is generic `PerformAbility`; it must not contain `if target is field mouse` behavior.

## Field mouse GDScript

Use the real typed script runtime to demonstrate:

- pre-resolution attack rule contribution;
- post-damage or post-death reaction;
- persistent state flag;
- deterministic random or deferred bark command;
- no direct health or combat mutation.

Keep behavior legible enough to serve as an author example.

## Door

The door:

- is a real ECS entity;
- anchors to one or more edges;
- is not required to reach the mouse;
- contributes traversal restrictions based on state;
- commits open or closed state before animation;
- saves and replays;
- has a view that can fail or timeout without changing domain state.

## Tests first

Combat core:

- deterministic initiative and tie breaker;
- turn start and AP refresh;
- non-active actor command rejection;
- movement AP integration;
- Thump range, target, AP, hit, damage, and death paths;
- rejected Thump leaves AP and health unchanged;
- rule contribution ordering including GDScript;
- event order and causation;
- dead actor skipped;
- combat end transition;
- combat machine snapshots and restore;
- seeded replay.

Door core:

- edge footprint validation;
- open and close legal transitions;
- locked or invalid transition policy if implemented;
- traversal blocked closed and allowed open;
- state commits before presentation acknowledgement;
- missing, duplicate, and delayed acknowledgement leave domain state identical;
- save and restore;
- door route is not part of shortest required path to mouse.

Godot:

- typed AbilityDefinition compiles;
- field mouse script receives typed events;
- Thump and door views animate from presentation events;
- combat grid and path preview use core data;
- animation timeout fallback works.

## Prohibited shortcuts

- field-mouse-specific C++ in generic ability handling;
- health changed by animation callback;
- AP stored or spent in UI;
- door state stored in Node animation or visibility;
- team turns substituted without decision;
- random call through Godot;
- event logs used instead of typed facts;
- quest, inventory, or dialogue scaffolding.

## Validation

```bash
./bin/dross generate all
./bin/dross check generated
cmake --build --preset linux-debug
ctest --preset linux-debug --output-on-failure
dross_headless scenario thump-on-field-mouse --seed 12345 --record build/thump.dross-replay
dross_headless replay --verify-checkpoints build/thump.dross-replay
godot --headless --path godot -s res://tests/run_phase12.gd
```

## Suggested commits

1. `test: specify initiative turn and AP policy`
2. `feat: add combat session and turn FSMs`
3. `test: specify generic ability and Thump resolution`
4. `feat: add typed ability combat pipeline`
5. `test: specify edge-anchored door lifecycle`
6. `feat: add door capability and traversal rules`
7. `feat: add field mouse script and safe presentation cues`

## Exit criteria

- the headless scenario completes real combat;
- Thump is data plus generic capability primitives;
- door is optional and edge-anchored;
- SML controls live combat and door state;
- GDScript contributes and reacts through typed APIs;
- animation cannot change outcomes;
- save codecs exist for new persistent state;
- no unrelated RPG system was introduced.

## Stop conditions

- combat and movement AP cannot share one movement model;
- ability effect composition requires an unresolved runtime scripting model;
- door edge state cannot update traversal without bypassing ownership;
- presentation gating cannot be separated from domain commit under current adapter.
