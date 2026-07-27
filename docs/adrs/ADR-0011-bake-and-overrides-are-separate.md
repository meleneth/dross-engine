# ADR-0011: Generated Bake Facts and Human Overrides Are Separate

Status: Accepted

## Context

Automatic geometry analysis is useful but imperfect. Authors need manual corrections that survive rebakes. Combining generated and manual data allows tools to erase intent.

## Decision

Store generated `DrossHexGridBake` and human `DrossHexGridOverrides` separately. Compile both into immutable `DrossCompiledHexMap`.

## Consequences

- the editor must display provenance and orphaned overrides;
- rebakes are safe and diffable;
- runtime depends only on compiled data;
- map validation has a clear merge stage.

## Enforcement

- generated assets are not hand-edited;
- overrides use stable cell and edge addresses;
- tests rebake and preserve an override;
- runtime pathfinding never queries editor geometry.
