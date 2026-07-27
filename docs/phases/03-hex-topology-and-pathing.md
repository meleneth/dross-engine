# Phase 03: Hex Topology, Footprints, and Path Planning

## Goal

Implement the Godot-free pointy-top hex model, cells, edges, layers, footprints, occupancy, and deterministic weighted path planning over `HexPose`.

## Read first

- `docs/06-hex-grid-model.md`
- ADR-0005, ADR-0011
- `docs/20-open-decisions.md` pathfinding section

## Live production consumer

`dross_headless scenario hex-pathing` loads a small synthetic compiled map, places a single-cell actor and an asymmetric two-cell test entity, and prints deterministic paths, costs, rotations, and blocked reasons.

## Scope

Implement:

```text
HexCoord and cube helpers
HexDirection and HexFacing
HexCellId and HexPose
EdgeKey and directional edge facts
CellFacts and CompiledHexMap core DTO
FootprintDefinition and rotated expansion
static and dynamic occupancy index
TraversalPolicy and TraversalAssessment
PathPlanner interface and one real deterministic weighted A* implementation
synthetic map fixture builder
```

Editor bake resources arrive in phase 10. The core map representation is established here.

## Path library spike

Before committing the planner, perform a small spike using the asymmetric footprint and rotation state to evaluate whether Boost.Graph or another maintained library genuinely reduces code and preserves deterministic tie-breaking.

Accept a custom domain A* only when:

- it uses established standard containers rather than custom heap structures;
- the search logic is isolated behind `PathPlanner`;
- optimality and tie-breaking are exhaustively tested;
- the spike result is documented in a small ADR or phase note.

Do not choose a generic library merely to satisfy a checkbox if its adapter is larger and less testable than the domain algorithm.

## Map representation

Use explicit cells and edges. Missing neighbor edges mean no traversal. Layer transitions are representable as explicit edges, even though fixtures stay flat.

Cell and edge collections expose deterministic canonical iteration independent from insertion order.

## Footprint

The two-cell fixture must be asymmetric and require a facing-aware rotation to traverse at least one route. This prevents fake multi-hex support.

Rotation is a first-class path transition with policy cost.

## Occupancy

The occupancy index is derived state owned by the hex capability. It is updated through explicit placement, move commit, and removal methods in this phase. Event-driven updates arrive after phase 04.

Test rebuild from world state versus incremental updates.

## Tests first

Property and exhaustive tests:

- axial/cube round trips;
- distance properties;
- direction opposite and rotation cycles;
- canonical edge equality from either endpoint;
- same `(q, r)` on different layers remains distinct;
- footprint validation and six rotations;
- placement expansion;
- occupancy conflict detection;
- blocked cells and directional edges;
- path optimality on all small bounded maps practical for exhaustive enumeration;
- deterministic tie-breaking across insertion orders;
- asymmetric footprint route and rotation;
- no path through a closed edge fixture;
- path invalidation when occupancy revision changes.

## Numeric policy

Use integer costs. Heuristics must be admissible under the selected movement policy. If rotation and terrain costs complicate the heuristic, begin with a conservative admissible heuristic rather than an incorrect faster one.

## Prohibited shortcuts

- square grid hidden behind hex rendering;
- cell-only door or traversal model;
- path nodes that ignore facing;
- footprint represented as radius;
- Godot `AStar3D`, navigation mesh, or physics in core;
- floating-point path cost;
- relying on unordered iteration for tie-breaking;
- a single center-cell collision check for multi-hex placement.

## Validation

```bash
cmake --build --preset linux-debug
ctest --preset linux-debug --output-on-failure
dross_headless scenario hex-pathing
```

Run property tests with a recorded seed on failure and enough cases to exercise shrinking. Run sanitizers.

## Suggested commits

1. `test: specify pointy-top hex and footprint invariants`
2. `feat: add compiled hex topology and occupancy`
3. `test: prove deterministic pose-aware path planning`
4. `feat: add weighted hex pose planner`

## Exit criteria

- single and multi-hex pathing use the same APIs;
- facing and rotation affect legality;
- cells and edges are distinct concepts;
- path results are optimal and deterministic for test domains;
- no Godot dependency exists;
- one live headless scenario consumes every introduced major abstraction.

## Stop conditions

- pathfinding library choice prevents deterministic tie-breaking;
- pointy-top projection cannot round-trip through chosen integer quantization;
- the map representation cannot express overlapping layers without replacing cell identity;
- occupancy requires direct mutation outside its owner.
