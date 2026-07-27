# Phase 01: Foundation Value Types

## Goal

Implement the strong, deterministic core value types used by every later capability. Prove validation, ordering, canonical formatting, and overflow behavior without building world behavior yet.

## Read first

- `docs/01-architectural-invariants.md`
- `docs/02-domain-language.md`
- `docs/09-rng-fixed-tick-replay.md` for numeric constraints
- ADR-0018

## Live production consumer

The `dross_headless` executable introduced in this phase parses and prints a validated engine/content identifier and a small deterministic foundation self-check. It is intentionally small but real.

## Scope

Implement:

```text
ContentId
EntityId
WorldInstanceId
Tick
CommandId
CausationId
CorrelationId
RegionId
strong integer quantities needed immediately
Result alias and foundational error records
stable byte writer and reader primitives for later canonical encoding
```

`HexCoord` belongs to phase 03. RandomHub belongs to phase 05.

## ContentId

Requirements:

- canonical form `namespace:name`;
- normalized ASCII policy documented and validated;
- no empty namespace or name;
- no hidden case folding after creation;
- stable lexical ordering by canonical string;
- canonical string retained, not only hash;
- optional cached stable hash uses BLAKE3 or another accepted fixed algorithm, never `std::hash`;
- useful parse errors with position and reason.

Decide the exact allowed character set in tests before implementation. A conservative initial set such as lowercase ASCII letters, digits, `_`, `-`, `.`, and `/` is acceptable.

## EntityId allocation primitives

Implement value representation and ordering, but not the world allocator yet. Provide canonical byte encoding and display format.

## Strong quantities

Implement only quantities used by upcoming phases, likely:

```text
Tick
Millimeters
MovementCost
ActionPoints
HitPoints
Initiative
```

Prefer a small established strong-type library if it meets serialization and warning requirements. Otherwise use narrow Dross wrappers with explicit arithmetic and overflow checks. Do not create a broad units framework with no consumer.

## Stable encoding primitives

Provide a simple canonical byte writer and reader for fixed-width integers, byte strings, and validated ContentIds. This is not the complete save codec. It supports golden hashes and IDs later.

Specify byte order explicitly, normally little-endian for the container or network-order if selected. Once fixtures exist, changing it requires migration.

## Tests first

Required failing tests before implementation:

- ContentId accepted and rejected forms;
- parse error positions;
- lexical ordering;
- stable canonical bytes and hash golden vector;
- EntityId ordering and round trip;
- Tick checked addition and overflow behavior;
- strong quantities cannot be mixed accidentally at compile time where practical;
- stable writer output golden bytes;
- reader rejects truncation and invalid lengths;
- no locale-dependent formatting.

## Headless executable

Create `dross_headless` with:

```bash
dross_headless version
dross_headless validate-id dross:thump
```

The CLI must return useful exit codes and avoid adding a heavyweight custom argument parser if an accepted library is already available or a minimal subcommand count can use a tiny explicit parser. Do not let CLI design consume the phase.

## Prohibited shortcuts

- raw strings throughout the API in place of ContentId;
- `std::hash` for persisted or deterministic identity;
- platform-size `long` in encoded formats;
- unchecked integer casts;
- a generic `Variant` foundation type;
- premature UUID library adoption for deterministic entity IDs;
- broad quantity templates that obscure errors and debugger output.

## Validation

Run:

```bash
cmake --build --preset linux-debug
ctest --preset linux-debug --output-on-failure
dross_headless version
dross_headless validate-id dross:thump
```

Run sanitizer and clang-tidy on new sources.

## Suggested commits

1. `test: specify stable content and identity values`
2. `feat: add strong deterministic foundation values`
3. `feat: add headless foundation command surface`

## Exit criteria

- value types have canonical ordering and encoding;
- invalid data cannot enter through public constructors without a Result;
- golden vectors pass;
- no Godot types or dynamic variants enter core;
- `dross_headless` is a real consumer;
- no unused value type was added for speculative systems.

## Stop conditions

- stable hash library differs across supported targets;
- chosen strong-type approach produces unacceptable diagnostics or serialization leakage;
- the ContentId syntax conflicts with Godot `StringName` round trips in a material way, which should be reproduced before changing syntax.
