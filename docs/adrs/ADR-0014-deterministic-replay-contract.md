# ADR-0014: Replay Is Deterministic Across Supported Platforms

Status: Accepted

## Context

A seed alone does not encode player choices, but deterministic simulation should not require recording random outcomes. Reproducible failures are a primary engine goal.

## Decision

Given the same engine and content versions, initial snapshot, master seed, and tick-stamped external command stream, Windows, Linux, and Steam Deck builds must produce the same command results, events, machine transitions, and canonical state hashes.

## Consequences

- authoritative arithmetic avoids platform-sensitive floating-point decisions;
- ordering must be stable;
- replay logs record commands, not RNG results;
- platform CI needs shared golden fixtures.

## Enforcement

- canonical state hashing at checkpoints;
- PCG and hash golden vectors;
- replay divergence localization;
- no reliance on unordered iteration or standard random distributions.
