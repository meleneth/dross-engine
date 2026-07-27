# Phase 11: Real-Time Hex Exploration Movement

## Goal

Implement player command input, path preview, fixed-tick real-time traversal, movement SML lifecycle, dynamic occupancy updates, view interpolation, cancellation, and safe combat-pending behavior on the compiled demo map.

## Read first

- `docs/06-hex-grid-model.md`
- `docs/08-fsm-architecture.md`
- `docs/11-godot-boundary.md`
- ADR-0005, ADR-0006, ADR-0020

## Live production consumer

In the demo room, the player hovers cells, sees path and cost, clicks a destination, and watches the actor traverse the authoritative path. Movement can be cancelled or replaced at a cell boundary. A scripted combat request while moving causes the current edge to finish, blocks new exploration movement, and transitions mode to combat.

## Scope

Implement:

```text
MoveTo and CancelMovement generated commands
MovementStarted, ActorEnteredCell, MovementCompleted live events as needed by real consumers
movement planning and stale-plan revision checks
MovementLifecycle SML machine and host
MovementState persistent component or snapshot-owned context according to phase 06 pattern
fixed-tick edge duration
occupancy commit policy
path preview query DTO
Dross movement command and query bindings
DrossEntityView and DrossViewRegistry
presentation snapshots and interpolation
selection conversion from Godot raycast to cell ID
combat pending safe-boundary integration
save codecs for movement state
```

Only emit events that have a live consumer such as occupancy, script, view, combat pending, or trace.

## Movement semantics

- `MoveTo` validates current pose, destination, footprint, occupancy, and policy;
- path plan captures relevant map and occupancy revision;
- each transition has fixed tick duration;
- authoritative destination commits at the defined cell boundary;
- dynamic occupancy changes invalidate future transitions and drive blocked or replan policy;
- cancel takes effect at a safe pose boundary;
- combat pending finishes the current edge then stops;
- no new exploration command is accepted while pending;
- views interpolate from tick metadata and never write back.

## Movement FSM

States must correspond to real invariants. A likely path:

```text
Idle + MoveAccepted -> Planning or Traversing
Traversing + TransitionCompleted -> Traversing or Completing
Traversing + CancelRequested -> Cancelled at safe boundary
Traversing + PathInvalidated -> Blocked
Completing + Completed -> Idle
Blocked + ReplanAccepted -> Traversing
Cancelled + Settled -> Idle
```

Reduce or adjust states if a test proves no distinct behavior. Use SML, snapshot, restore, and trace.

## Path preview

Preview uses the same planner and rule assessment as command validation. It returns:

```text
path poses
movement cost
duration
blocked or rejection reason
occupancy and map revision
rotation steps
```

The preview is advisory. `MoveTo` validates again.

## Tests first

Core:

- accepted path movement over fixed ticks;
- occupancy leaves and enters cells at correct boundaries;
- multi-hex occupancy during rotation and transition;
- cancel at boundary;
- stale path rejection or deterministic replan;
- dynamic blocker appears mid-route;
- combat pending finishes current edge and rejects later movement;
- movement machine snapshot and restore mid-route;
- save and resume movement produces same final hash;
- preview and command share assessment;
- event order and causation;
- replay equality.

Godot:

- hover resolves correct cell;
- path overlay matches core preview;
- view interpolation does not change core pose;
- missing view does not stop movement;
- deleting view and recreating it reconstructs from presentation snapshot;
- frame-rate variation does not change final state or tick trace.

## Prohibited shortcuts

- updating core position from `Node3D.global_position`;
- using Godot navigation for path cost;
- movement by arbitrary floating transform with cell snap at end;
- separate exploration pathfinder;
- animation or tween completion driving cell commit;
- cancel at fractional authoritative position;
- path preview implementation separate from command rules.

## Validation

```bash
cmake --build --preset linux-debug
ctest --preset linux-debug --output-on-failure
dross_headless scenario exploration-movement --seed 12345 --record build/movement.dross-replay
dross_headless replay --verify-checkpoints build/movement.dross-replay
godot --headless --path godot -s res://tests/run_phase11.gd
```

Perform interactive smoke at varied frame rates if practical.

## Suggested commits

1. `test: specify fixed-tick movement and occupancy`
2. `feat: add MoveTo command and movement SML machine`
3. `test: specify cancel stale path and combat pending`
4. `feat: add movement preview and Godot command bindings`
5. `feat: add entity views and deterministic interpolation`

## Exit criteria

- movement works headlessly and visually through the same core path;
- occupancy and multi-hex rules remain correct;
- combat pending behavior is deterministic;
- movement state saves and restores;
- views are optional and non-authoritative;
- no second path or movement model exists.

## Stop conditions

- a pose transition cannot define safe occupancy semantics for multi-hex entities;
- movement machine state duplicates authoritative data without one source of truth;
- Godot frame scheduling changes command tick assignment nondeterministically;
- editor-compiled map lacks data required for movement validity.
