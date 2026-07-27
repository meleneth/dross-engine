# ADR-0007: pcg64 Behind RandomHub

Status: Accepted

## Context

Fairlanes already established PCG and `pcg64`. Dross needs named streams, snapshots, cross-platform samples, and deterministic replay.

## Decision

Use `imneme/pcg-cpp` `pcg64` exclusively through `RandomHub`. Derive named streams with a stable versioned hash. Use Dross sampling wrappers and PCG bounded algorithms.

## Consequences

- every capability declares its stream identity;
- stream states are persisted;
- standard random distributions and Godot RNG are forbidden for authoritative behavior;
- presentation randomness is separate.

## Enforcement

- source scans reject forbidden APIs;
- golden vectors run across supported compilers;
- replay verifies stream call sequences in developer mode;
- no direct `pcg64` construction outside RandomHub internals.
