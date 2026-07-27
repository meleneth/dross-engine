# Phase 10: Grid Editor Authoring

## Goal

Augment the Godot editor with a visible pointy-top grid region, geometry-derived bake, persistent manual overrides, compiled runtime map, diagnostics, and the same visible runtime overlay.

## Read first

- `docs/12-grid-authoring-editor.md`
- ADR-0011

## Live production consumer

Create the first arbitrary 3D demo room in Godot. The editor tool must:

- display its grid;
- bake walkable and blocked cells from collision geometry;
- preserve one manual override across rebake;
- compile a `DrossCompiledHexMap`;
- display the same cells through a runtime overlay;
- identify an optional edge for the later side door.

## Scope

Implement:

```text
DrossHexGridRegion3D
DrossHexBakeProfile Resource
DrossHexGridBake Resource
DrossHexGridOverrides Resource
DrossCompiledHexMap Resource adapter and core DTO conversion
native geometry analyzer interface with one real implementation
map merge and validation service
@tool EditorPlugin UI
EditorNode3DGizmoPlugin or equivalent 3D grid visualization
cell and edge selection
manual traversability paint for one cell
runtime DrossHexGridOverlay3D
CLI compile and validate command using serialized bake evidence where possible
```

Do not add terrain painting, line of sight, cover, or multi-level UI beyond what the data model already represents.

## Geometry fixture

The room must contain:

- flat traversable floor;
- one geometric obstruction that automatically blocks cells or edges;
- enough clearance variation to exercise diagnostics;
- one cell whose automatic classification is manually overridden;
- one optional side doorway edge not on the direct route to the future mouse.

## Analyzer quality

Use center plus inset surface samples, clearance checks, quantization, and edge transition checks as defined in the architecture document. A one-ray prototype is not accepted.

Store evidence and reason codes so the editor can explain classification.

## Override safety

- bake and override files are separate;
- changing geometry and rebaking retains compatible overrides;
- changing grid origin or radius surfaces orphaned overrides;
- author must explicitly migrate or remove orphans;
- compile rejects unresolved required conflicts.

## Editor interaction

The initial plugin needs a practical workflow, not a polished final tool:

1. select grid region;
2. bake;
3. inspect generated cells;
4. select a cell;
5. toggle force blocked or force traversable;
6. rebake;
7. compile;
8. run runtime overlay.

All actions show structured validation errors.

## Tests first

Core native tests:

- synthetic evidence to bake facts;
- quantization;
- merge precedence;
- orphan detection;
- deterministic compiled map regardless of input ordering;
- invalid edge and layer references;
- runtime core DTO round trip.

Godot tests:

- editor plugin loads;
- grid gizmo registers;
- demo room bake produces expected cell identities;
- manual override persists after rebake;
- compiled map hash remains stable for unchanged inputs;
- runtime overlay cell set equals compiled map cell set;
- selection maps to correct `HexCellId`;
- no runtime path query inspects physics geometry.

## Prohibited shortcuts

- same asset for bake and overrides;
- runtime pathing against Godot geometry;
- duplicated editor and runtime hex math;
- silent orphan override deletion;
- transform rounding to determine authoritative cell at runtime;
- a visual overlay that omits blocked cells used by the core;
- hand-authored map fixture replacing the required bake path.

## Validation

```bash
cmake --build --preset linux-debug
ctest --preset linux-debug --output-on-failure
godot --headless --path godot --editor --quit
godot --headless --path godot -s res://tests/run_phase10.gd
./bin/dross check generated
```

Also open the editor interactively and record the exact manual smoke steps completed.

## Suggested commits

1. `test: specify bake evidence merge and override safety`
2. `feat: add native grid bake and compile resources`
3. `feat: add Godot grid region and 3D gizmo`
4. `feat: add persistent cell override workflow`
5. `test: prove runtime overlay uses compiled map`

## Exit criteria

- one arbitrary 3D room compiles into the core map;
- automatic and manual classifications are distinguishable;
- rebake preserves manual intent;
- editor and runtime overlays use the same compiled cells;
- reason diagnostics exist;
- no future editor tool stubs exist.

## Stop conditions

- the pinned godot-cpp 4.7 API lacks a required editor gizmo or physics query API;
- Godot headless cannot exercise required editor behavior, requiring a documented interactive-only test strategy;
- stable cell addressing cannot survive rebake under the accepted grid transform model;
- analyzer outputs cannot be quantized consistently.
