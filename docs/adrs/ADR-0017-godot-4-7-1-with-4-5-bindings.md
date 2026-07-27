# ADR-0017: Godot 4.7.1 with Stable 4.5 godot-cpp Bindings

Status: Accepted

## Context

Godot 4.7.1 is the current stable runtime baseline. godot-cpp v10 is beta, while the 4.5 line is stable and GDExtension supports earlier-minor API targets on later Godot minors.

## Decision

Develop and test against Godot 4.7.1 while targeting the stable godot-cpp 4.5 API initially. Set the extension minimum compatibility to 4.5.

## Consequences

- newer native APIs are unavailable until an intentional upgrade;
- current renderer and editor fixes remain available;
- extension compatibility gets an explicit CI test;
- a missing API can trigger a focused ADR rather than an automatic beta adoption.

## Enforcement

- exact Godot and godot-cpp versions are recorded;
- CI downloads or installs the pinned Godot binary;
- use of a newer Godot API is a build failure;
- upgrade requires a superseding ADR.
