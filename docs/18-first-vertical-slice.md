# First Vertical Slice: Thump on Field Mouse

## Purpose

This slice proves the Dross architecture through one complete, tiny game loop. It is not a throwaway demo and not permission to build unrelated RPG systems.

## Scene

One arbitrary 3D room contains:

- a player actor;
- a field mouse actor;
- one door installed in a dividing wall and attached to a hex edge;
- one route from the player to the mouse that requires opening and crossing the door;
- geometry that produces at least one automatically blocked cell;
- one manually overridden blocked or traversable cell;
- visible hex overlay support.

The map is authored in Godot and compiled into one `DrossCompiledHexMap`.

## Exploration

The player can:

1. hover a hex and inspect reachability;
2. click a reachable destination;
3. submit `MoveTo`;
4. watch the actor interpolate between authoritative cell transitions;
5. cancel or replace movement at a safe boundary;
6. open the route door;
7. cross its authoritative edge to approach the field mouse.

Exploration is real time in presentation but fixed-tick and hex-discrete in the core.

## Combat transition

A region or field-mouse GDScript behavior requests combat when its authored condition becomes true, such as the player entering an aggression radius.

The simulation mode enters `CombatPending`. The actor finishes the current edge traversal, accepts no new exploration move, then enters turn-based combat from the committed cell.

This proves GDScript command submission without giving the script direct mode mutation.

## Combat

Initial policy:

- individual actor initiative;
- deterministic initiative order;
- stable `EntityId` tie breaker;
- AP refreshed at turn start;
- explicit `EndTurn`;
- dead actors are removed from future turns through combat rules;
- one player ability named `dross-demo:thump`;
- Thump requires an adjacent legal target and spends AP;
- hit and damage resolution use deterministic integer rules;
- the field mouse may die from the configured demo hit, but the architecture must not hard-code the target type.

Randomness should be present in one controlled combat result only if it adds a useful replay proof. A deterministic fixed damage Thump is acceptable in the earliest combat commit, followed by one seeded random roll before final slice acceptance.

## GDScript behavior

The field mouse has an entity script that demonstrates:

- one typed pre-resolution rule contribution;
- one typed post-event reaction;
- one persistent script state value;
- one deterministic random call or a paired demo script using the same runtime API;
- one deferred command.

A minimal example could make a tagged sacred mouse reject Thump, while the ordinary field mouse records that it was attacked and queues a bark or authored reaction. The actual demo behavior must remain understandable and testable.

## Door

The route door demonstrates:

- edge footprint;
- stable entity identity;
- C++ door lifecycle machine;
- `OpenDoor` and `CloseDoor` commands;
- traversal rule contribution from door state;
- typed events;
- Godot animation triggered after domain commit;
- save and reload;
- closed-door rejection of movement preview and movement commit;
- required traversal through the open doorway before combat becomes reachable.

## Presentation

Godot provides:

- orthographic isometric camera;
- arbitrary 3D models or simple production-intent stand-ins;
- visible runtime grid;
- cell hover and path preview;
- actor movement interpolation;
- Thump animation or clear action cue;
- field mouse reaction and death presentation;
- door animation;
- combat log or developer event view.

Presentation acknowledges action animation, but health, AP, door state, and death are already committed.

## Save and load

Demonstrate saves at:

- an exploration quiescent boundary;
- a combat actor-turn boundary;
- after the door state or mouse state changes.

Reload reconstructs the authoritative world and views. It does not serialize Godot nodes.

## Replay

Record a command stream that:

1. moves the player;
2. opens and crosses the route door;
3. triggers combat;
4. moves in combat if needed;
5. performs Thump;
6. ends or exits combat according to the demo rule;
7. saves or reaches a final checkpoint.

Replaying from the same initial snapshot and seed produces identical command results, events, machine transitions, RNG trace, and canonical state hashes on Linux and Windows CI.

## Headless proof

A headless scenario test performs the entire authoritative sequence with a fake script runtime and no Godot.

A separate Godot integration scenario proves:

- typed resources compile into the same definitions;
- real GDScript receives typed callbacks;
- views follow the resulting presentation snapshot;
- the editor-authored compiled map matches the fixture identity.

## Acceptance checklist

- [x] no Godot include in `dross_core`;
- [x] EnTT stores all authoritative entities and components;
- [x] eventpp carries live domain events with real listeners;
- [x] SML machines control world mode, movement, combat, and door lifecycles where those states are distinct;
- [x] visible editor and runtime grids use the same compiled map;
- [x] footprint and facing are authoritative;
- [x] asymmetric multi-cell footprint tests pass even though demo actors are single-cell;
- [x] GDScript cannot mutate components;
- [x] command rejection is side-effect free;
- [x] animation cannot cause or prevent domain mutation;
- [x] save and load preserve every required state;
- [x] replay matches canonical hashes on the validated Linux target;
- [x] all generators are clean and idempotent;
- [ ] Linux GCC, Linux Clang, sanitizer, Windows, and Godot headless CI pass;
- [x] no introduced abstraction is unused.

The evidence for each checked item, and the unavailable portions of the open
platform item, are recorded in
`docs/validation/phase14-acceptance-evidence.md`.
