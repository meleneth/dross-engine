# Phase 13: Complete Save, Reload, and Cross-Platform Replay

## Status

**Not finished.** Local Linux debug validation passes, but the required
cross-platform evidence is incomplete. In particular, Phase 13 does not yet
have recorded proof for every required compiler, sanitizer, Windows, Godot,
and Steam Deck validation target.

Work is proceeding to Phase 14 by explicit project-owner direction. Windows
and Steam Deck hardware are unavailable to the project, so those targets are
recorded as unverified rather than treated as blockers or reported as passing.
Available Linux and Godot validation remains required.

## Goal

Extend persistence and replay across every first-slice capability, add committed fixtures and migrations, and prove deterministic equivalence on Linux, Windows, and Steam Deck-compatible Linux builds.

## Read first

- `docs/09-rng-fixed-tick-replay.md`
- `docs/13-persistence.md`
- ADR-0013, ADR-0014

## Live production consumer

The full Thump scenario runs in three forms:

1. uninterrupted;
2. save during exploration, reload, continue;
3. save at a combat actor-turn boundary, reload, continue.

All forms reach the same final canonical state and event hashes. The recorded replay verifies on Linux GCC, Linux Clang, and Windows MSVC CI.

## Scope

Complete codecs and canonical state for:

```text
compiled map identity
all entities and stable identity allocator
hex pose, footprint references, occupancy rebuild
movement state and machine
world and simulation-mode machines
combat session and turn state
combatant, health, AP, initiative
ability or definition references
field mouse script bindings and state bag
door definition, edge footprint, state, and machine
RandomHub streams
current tick
content and mod manifest
external replay command stream
```

Add at least one meaningful migration fixture for a first-slice component schema change. Do not manufacture a migration that copies identical fields merely to check a box.

## Canonical inspection

Improve divergence output so a mismatch identifies the first differing section, entity, component type, machine, script state key, or random stream.

## Content manifest

Use real base and demo package IDs, versions, dependency order, and content hashes. Save and replay reject changed required content unless an explicit developer migration path exists.

## Cross-platform CI

Required jobs:

```text
Linux Clang debug and replay verify
Linux GCC release and replay verify
Linux ASan plus UBSan
Windows MSVC build, unit tests, and replay verify
Godot Linux headless integration
Godot Windows headless integration when runner support is available
```

Steam Deck validation uses the Linux release artifact plus a documented device smoke run. If no device runner exists, do not claim automated Deck validation.

## Tests first

- codecs for every persistent first-slice component;
- save at exploration and combat boundaries;
- save refusal during command or event phases;
- view and animation state excluded but reconstructed;
- script state and deterministic RNG restored;
- occupancy rebuilt equals uninterrupted occupancy;
- all SML machines restore from fixture states;
- content hash mismatch failure;
- missing demo package failure;
- meaningful migration fixture;
- uninterrupted and both resumed paths equal;
- Linux and Windows replay fixture golden hashes;
- malformed unknown required record failure;
- deterministic save bytes across repeated writes.

## Prohibited shortcuts

- updating golden hashes without finding the cause;
- platform-specific expected event order;
- serializing view nodes to make reload look correct;
- accepting content mismatch silently;
- excluding a difficult machine state from save support;
- claiming Steam Deck support without at least a real device smoke log;
- weakening canonical hash coverage to hide divergence.

## Validation

Local:

```bash
cmake --build --preset linux-debug
ctest --preset linux-debug --output-on-failure
dross_headless scenario thump-on-field-mouse --seed 12345 --record build/thump.dross-replay --save-checkpoints build/saves
dross_headless replay --verify-checkpoints build/thump.dross-replay
dross_headless compare-runs build/uninterrupted.trace build/resumed.trace
```

CI must verify the committed replay fixture on supported platforms.

## Suggested commits

1. `test: specify complete first-slice save schema`
2. `feat: persist movement combat door and script state`
3. `test: add meaningful save migration fixture`
4. `feat: localize canonical replay divergence`
5. `ci: verify Dross replay across Linux and Windows`

## Exit criteria

- every first-slice authoritative state is saved or deterministically rebuilt;
- uninterrupted and resumed runs match;
- Linux and Windows replay fixture matches;
- content manifest is real and enforced;
- migration fixture proves schema evolution;
- no Godot presentation state is serialized.

## Stop conditions

- any supported platform produces a different PCG, event, or canonical hash sequence;
- a machine cannot restore from a legitimate quiescent state;
- Godot resource import changes content hash nondeterministically without a compiled DTO solution;
- save compatibility requires silent data loss.
