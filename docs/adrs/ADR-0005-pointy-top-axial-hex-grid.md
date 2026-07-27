# ADR-0005: Pointy-Top Axial Hex Grid

Status: Accepted

## Context

The game uses arbitrary 3D rendering but discrete movement and command input. Hex movement offers six natural directions and readable turn-based positioning.

## Decision

Use pointy-top axial `(q, r)` coordinates with cube helpers. The horizontal world plane is Godot X/Z and physical elevation is Y. Cells include region and layer identity. Facing has six authoritative values.

## Consequences

- projection and quantization are centralized;
- overlapping layers are representable;
- path state includes facing;
- editor and runtime overlays use one compiled map.

## Enforcement

- pointy-top direction order is serialized and tested;
- authoritative position is never a `Vector3`;
- six rotations and projection properties have exhaustive tests;
- changing orientation requires a superseding ADR and migration.
