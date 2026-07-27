# ADR-0010: Integrate Through GDExtension

Status: Accepted

## Context

A Godot source module offers deeper integration but requires custom engine and export-template builds. Dross needs native C++ integration while keeping Godot replaceable and build times manageable.

## Decision

Use GDExtension through official godot-cpp bindings. Build `dross_godot` as a shared library linked to `dross_core`.

## Consequences

- all exposed classes require explicit ClassDB registration;
- extension compatibility must be tested with the chosen Godot version;
- APIs unavailable to GDExtension may require later reconsideration;
- the same library can be used in editor and exported project.

## Enforcement

- no Godot source fork in initial phases;
- missing required GDExtension functionality is a stop condition;
- extension loading is tested headlessly;
- core remains independently buildable.
