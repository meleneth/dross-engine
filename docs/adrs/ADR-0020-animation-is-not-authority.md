# ADR-0020: Animation Is Not Authority

Status: Accepted

## Context

Waiting for animation callbacks to apply domain effects creates missing-signal, timing, replay, and save bugs. Presentation may still need to gate input for readability.

## Decision

Commit authoritative action results before presentation animation completes. Animation acknowledgements may release a presentation gate but cannot decide health, AP, position, door state, death, or quest outcomes.

## Consequences

- presentation events need enough data to play after commit;
- save and reload can reconstruct visuals from domain state;
- missing animations need fallback acknowledgement;
- combat pacing and domain mutation are separate concerns.

## Enforcement

- animation callback APIs expose acknowledgement only;
- integration tests omit or delay an animation and verify identical core state;
- no command handler is invoked from an animation completion callback;
- timeouts are presentation policy, not domain rollback.
