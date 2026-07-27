# ADR-0012: Stable Entity Identity Is Separate from EnTT Handles

Status: Accepted

## Context

EnTT handles are process-local storage details. Saves, scripts, replays, editor references, and unloaded views need stable identity.

## Decision

Use a persistent `EntityId` composed from world lineage and deterministic sequence. Use optional namespaced aliases for authored convenience. Expose validated `EntityRef` values containing world instance and stable ID.

## Consequences

- identity indexes must be maintained and rebuilt;
- destroyed IDs are not reused;
- authoring assigns durable placed-entity identity;
- cross-load live references become invalid by world instance.

## Enforcement

- no raw EnTT handle crosses the core boundary;
- saves store stable IDs;
- ID allocation has deterministic tests;
- aliases cannot be the sole persistent identity.
