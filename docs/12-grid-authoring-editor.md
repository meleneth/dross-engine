# Grid Authoring and Editor Integration

## Goal

Authors build arbitrary 3D maps in Godot while Dross displays, compiles, validates, and runs a visible logical hex grid over that geometry.

The editor workflow must support automatic geometry analysis and durable human overrides without maintaining two competing grid truths.

## Authoring node

A C++ GDExtension `DrossHexGridRegion3D` node marks one logical region and exposes:

```text
RegionId
pointy-top orientation
cell radius
region origin and basis
horizontal bounds or authoring volume
layer rules
bake profile resource
geometry collision mask
compiled map output path
override resource path
overlay display settings
```

The transform positions the grid authoring frame. Runtime cells retain region-local logical addresses and compiled projection data.

## Asset split

The editor produces three distinct assets.

### `DrossHexGridBake`

Generated evidence from geometry analysis:

- sampled surfaces;
- integer heights;
- clearance results;
- slope or height variance;
- candidate neighbor transitions;
- blocked-cell and blocked-edge reasons;
- geometry source identity where available;
- bake profile and algorithm version.

Authors do not hand-edit this asset.

### `DrossHexGridOverrides`

Human intent:

- force traversable or blocked cell;
- override terrain or movement cost;
- add semantic tags;
- force or block a directional edge;
- attach authored door or barrier identity;
- override layer or transition metadata;
- suppress or acknowledge diagnostics.

Rebaking does not erase this asset.

### `DrossCompiledHexMap`

The deterministic merge of bake facts, overrides, and content validation. This is the immutable runtime input used by:

- path planning;
- occupancy;
- movement preview;
- runtime visible grid;
- line and edge selection;
- save references;
- validation diagnostics.

The runtime does not inspect scene collision to reconstruct this map.

## Bake profile

`DrossHexBakeProfile` is a typed Resource containing author-controlled geometry rules, such as:

```text
vertical probe range
center and inset sample pattern
maximum surface height variance
maximum slope
required standing clearance
maximum automatic step height
collision masks
recognized walkable and blocking tags
edge sweep settings
quantization units
```

The profile is versioned and included in bake identity.

## Initial geometry analyzer

The first real analyzer should be robust enough to continue using, not a single center ray that immediately needs replacement.

For each candidate cell:

1. sample the center and a configurable ring of inset points;
2. identify a coherent supporting surface;
3. quantize surface height to integer millimeters;
4. reject excessive height variance or slope;
5. perform clearance checks for the baseline actor category;
6. retain evidence and reason codes;
7. generate candidate edges to neighboring cells;
8. test edge transition clearance and height change;
9. record directional traversal facts.

The analyzer is an interface with one live implementation. The interface is justified because later bake profiles and geometry strategies are already part of the same production editor workflow, not because an empty plugin system seems elegant.

## Overrides and stable addressing

Overrides address cells through `HexCellId` and edges through canonical `EdgeKey`. They survive geometry rebakes as long as the grid origin, radius, region ID, and coordinate mapping remain compatible.

When those fundamentals change, the editor shows orphaned overrides and requires explicit migration or deletion. It never silently applies an override to a different cell.

## Editor plugin

A typed `@tool` GDScript `EditorPlugin` provides workflow UI:

- select a grid region;
- preview candidate cells and edges;
- bake or rebake;
- view bake differences;
- paint cell traversability overrides;
- paint terrain or costs;
- paint edge blocks or permissions;
- attach a door entity to an edge;
- inspect reason codes;
- compile and validate the map;
- jump to invalid content.

C++ provides shared hex math, bake analysis, map merge, and validation. GDScript provides editor controls and author interaction.

## 3D gizmo and overlay

Use Godot's 3D gizmo plugin support so the grid is visible in the editor viewport.

The overlay must distinguish at least:

```text
automatically traversable
automatically blocked
manual force traversable
manual force blocked
invalid or orphaned override
selected cell
selected edge
```

Exact colors and materials are presentation choices. Semantic states are tested through generated overlay data rather than pixel colors.

## Runtime visible grid

The player can see the logical grid when the game requests it, particularly during path preview and combat.

The runtime overlay consumes `DrossCompiledHexMap` and dynamic query results:

- reachable cells;
- selected path;
- AP cost;
- blocked reason;
- target range;
- occupied footprint cells;
- current facing and rotation choices.

There is no independently generated runtime mesh with slightly different cell inclusion logic.

## Door authoring

A placed door visual has a Dross authoring component that selects one or more compiled edges. Compilation creates or configures an edge-anchored authoritative entity with:

- stable entity identity;
- `DrossDoorDefinition`;
- `EdgeFootprint`;
- initial door state;
- visual scene reference;
- optional entity script module.

The first demo door sits on an optional side boundary. The player can interact with it, but reaching the field mouse does not require opening or crossing it.

## Determinism boundary

Editor geometry analysis may use Godot physics and floating-point queries. Its output is quantized and compiled into stable data. Runtime determinism begins at `DrossCompiledHexMap`, not at the original physics query.

A bake is reproducible for the same Godot version, project geometry, bake profile, and analyzer version as a development goal. Replay does not rerun the bake.

## Validation

Compilation rejects:

- duplicate region or cell identity;
- invalid pointy-top coordinate mapping;
- edges referencing missing cells;
- invalid footprint definitions;
- orphaned required overrides;
- doors attached to nonexistent or nonadjacent edges;
- content IDs that fail namespace rules;
- inaccessible authored spawn cells;
- conflicting force-blocked and force-traversable intent;
- non-quantizable authoritative values.

Errors are structured and surfaced in both CLI and Godot editor.

## Editor tests

Headless native tests cover bake merge and validation with synthetic geometry evidence. Godot editor tests cover:

- plugin loading;
- gizmo registration;
- region selection;
- one automated bake fixture;
- painting and saving a blocked-cell override;
- rebaking without losing the override;
- compiling identical runtime map data from the saved assets;
- attaching the optional door to an edge.
