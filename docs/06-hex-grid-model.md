# Hex Grid Model

## Chosen geometry

Dross uses **pointy-top axial coordinates** because they provide a conventional mapping, clean six-direction rotation, and a simple projection onto the Godot X/Z plane.

Stored logical coordinates:

```cpp
struct HexCoord {
    std::int32_t q;
    std::int32_t r;
    auto operator<=>(const HexCoord&) const = default;
};
```

Derived cube coordinates:

```text
x = q
y = -q - r
z = r
```

Cube coordinates are helpers for distance, rotation, and interpolation tests. They are not separately persisted.

## Layers and regions

Arbitrary 3D geometry requires more than `(q, r)`.

```cpp
struct HexCellId {
    RegionId region;
    HexCoord coord;
    std::int32_t layer;
};
```

`layer` distinguishes surfaces that overlap in X/Z, such as a bridge over a floor. Physical editor height is stored as an integer quantity such as millimeters in `CellFacts`.

The first map uses one region and one layer, but all APIs use `HexCellId` from the beginning.

## Directions and facing

Define six stable directions in clockwise order. The exact names and vectors are locked by tests and serialization.

Example ordering:

```text
0 East       (+1,  0)
1 Southeast  ( 0, +1)
2 Southwest  (-1, +1)
3 West       (-1,  0)
4 Northwest  ( 0, -1)
5 Northeast  (+1, -1)
```

The visual compass labels depend on the camera and world axes, but integer direction values do not change after saves exist.

`HexFacing` uses the same six-step rotation domain.

## Projection

The core provides a Godot-free projection value:

```cpp
struct WorldPointMm {
    std::int64_t x;
    std::int64_t y;
    std::int64_t z;
};
```

Given cell radius `s`:

```text
x = s * sqrt(3) * (q + r / 2)
z = s * 3/2 * r
y = baked surface height
```

The editor adapter may use floating-point math to draw geometry, but authoritative mapping uses a documented quantization strategy and baked integer positions. A cell's runtime identity never comes from rounding an arbitrary actor transform.

## Hex pose

```cpp
struct HexPose {
    HexCellId anchor;
    HexFacing facing;
};
```

Pathfinding, placement, occupancy, and combat range operate on poses when footprint orientation matters.

## Footprints

A footprint definition contains canonical offsets at facing zero:

```cpp
struct FootprintDefinition {
    FootprintId id;
    std::vector<HexCoord> occupied_offsets;
};
```

Requirements:

- offset `(0, 0)` must be present;
- duplicates are rejected;
- offsets are canonicalized in deterministic order;
- rotation by six steps returns the original set;
- placement expands offsets around the anchor and verifies all cells exist;
- asymmetric footprints are included in the first headless tests even though the player and mouse are one cell.

Do not encode footprint as radius. Non-circular shapes are expected.

## Rotation as movement state

Path state is `(anchor, facing)`. Neighbor generation may include:

- traverse a map edge while preserving or changing facing according to policy;
- rotate clockwise in place;
- rotate counterclockwise in place.

Rotation cost is policy-driven. The initial actor may rotate at zero exploration cost and a configurable combat AP cost. The pathfinder understands rotation from the start so multi-hex entities do not require a second path model later.

## Cells

`CellFacts` include, at minimum:

```text
cell ID
surface height in integer millimeters
terrain definition ID
base traversal cost in ticks
base traversal cost in AP units or a conversion policy
clearance category
traversable flag
semantic tags
bake evidence and diagnostic reasons
```

Generated facts and author overrides remain separate until compile.

## Edges

An edge is canonicalized from two adjacent cell IDs. It stores directional facts because climbing or one-way traversal may differ by direction.

```text
canonical edge identity
direction A -> B traversal facts
direction B -> A traversal facts
base movement cost
height delta or transition kind
blocking evidence
line-of-sight behavior
attached edge entity IDs
semantic tags
```

A door entity has an `EdgeFootprint`, which may cover one or more canonical edges. Door open or closed state contributes to traversal rules. The edge data itself does not become mutable door state.

## Compiled map

`CompiledHexMap` is immutable after load and supports:

- cell lookup;
- explicit neighbor enumeration;
- edge lookup;
- projection information;
- layer transitions;
- bake and override provenance for diagnostics;
- deterministic iteration order;
- canonical serialization and hashing.

Dynamic occupancy, doors, actors, and hazards are separate world state layered onto static map facts.

## Occupancy

The hex-world capability owns a derived occupancy index keyed by `HexCellId`.

Placement rules consider:

- static cell availability;
- footprint-expanded cells;
- dynamic blocking entities;
- edge blockers for transitions;
- capability-specific allowances, such as sharing a cell with a nonblocking marker.

The index is rebuilt deterministically after load and updated only through committed movement, spawn, destruction, or footprint-change events.

## Path planning

Dross requires weighted A* over an implicit graph of `HexPose` states.

The graph is domain-specific because neighbor validity depends on:

- footprint shape and facing;
- dynamic occupancy;
- directional edge facts;
- door and barrier state;
- movement capability;
- exploration or combat policy;
- rotation cost;
- future layer transitions.

Use standard-library containers or an accepted graph library for data structures. Do not invent a custom heap. Keep the path search behind a `PathPlanner` interface so an established library can replace the implementation without changing movement commands.

The first implementation must prove:

- optimal cost for small exhaustive maps;
- deterministic tie-breaking;
- no overlap for asymmetric footprints;
- rotation-aware paths;
- blocked edge and blocked cell behavior;
- path invalidation when occupancy changes;
- stable results across repeated runs.

Tie-breaking order is explicit: total estimated cost, actual cost, anchor cell order, facing, then insertion sequence.

## Real-time traversal

Exploration remains logically discrete.

- `MoveTo` plans a sequence of pose transitions.
- movement advances in fixed ticks.
- each edge has a deterministic duration.
- the presentation interpolates between committed poses.
- authoritative occupancy commits at the destination boundary defined by the movement policy.
- cancel and repath occur at safe pose boundaries.
- when combat becomes pending, the current edge traversal finishes, no new exploration move begins, and combat starts from the newly committed pose.

No actor has an authoritative position of `47 percent` through a hex edge.

## Combat traversal

Combat uses the same planner and transition primitives, with a combat movement policy that:

- limits reachable cost by AP;
- charges AP as transitions commit;
- exposes path and cost preview;
- rejects stale paths when relevant world facts changed;
- emits typed movement and AP events.

There is one movement model with policy variation, not two separate systems.
