# Phase 07: Persistence Foundation

## Goal

Implement explicit versioned save DTOs, component codec registration, canonical save output, migrations, fresh-world load, and quiescent-boundary checks for the world implemented so far.

## Read first

- `docs/13-persistence.md`
- ADR-0013

## Live production consumer

The existing headless scenario must:

1. run several ticks and commands;
2. save at a quiescent boundary;
3. continue uninterrupted to a final hash;
4. separately load the save into a fresh runtime;
5. continue with the same later commands;
6. reach the same final hash and event sequence.

## Scope

Implement:

```text
save container header and current codec
component type ID registry
DTO and codec for PersistentIdentity and current hex pose or placement components
identity allocator snapshot
CompiledHexMap reference and hash
WorldLifecycle and SimulationMode machine snapshots
RandomHub snapshot
fresh-world load plan
one migration fixture from a deliberately simple V0 to V1 record
save and load dross_headless commands
```

Use an established JSON library for the initial readable container if selected. The DTO and archive boundary must not expose JSON objects to capability code.

## Quiescent boundary

The runtime provides an explicit query and refusal reason. Tests attempt save during:

- active command commit fixture;
- event queue drain fixture;
- normal exploration boundary;
- combat mode boundary as currently modeled.

Do not use a mutex or sleep to guess safety.

## Codec registry

Registration is explicit and rejects duplicate stable type IDs. Each current persistent component has:

- version;
- encode;
- validate;
- migrate;
- load-plan construction;
- canonical hash contribution.

No empty codec slots for future components.

## Load plan

Parse and migrate without touching the current world. Construct a new world only after all records validate. Swap only after the new runtime passes invariants.

A failed load leaves the current runtime unchanged.

## Tests first

- byte-identical save of equivalent state;
- save refusal outside quiescent boundary;
- duplicate codec registration;
- current round trip;
- V0 fixture migration to V1;
- truncated and malformed container errors;
- missing map hash failure;
- invalid entity reference failure;
- machine snapshot restore;
- RandomHub state restore;
- failed load leaves old world hash unchanged;
- uninterrupted versus save-load continuation equality;
- replay beginning from loaded snapshot.

## Prohibited shortcuts

- serializing raw registry or components by memory dump;
- storing EnTT handles;
- mutating live world while parsing;
- silent default values for missing required fields;
- serializing trace sinks or listeners;
- storing SML internals;
- JSON objects passed through capability APIs;
- ignoring content or map identity.

## Validation

```bash
cmake --build --preset linux-debug
ctest --preset linux-debug --output-on-failure
dross_headless scenario persistence-foundation --seed 12345 --save build/foundation.dross-save
dross_headless inspect-save build/foundation.dross-save
dross_headless resume build/foundation.dross-save --commands tests/fixtures/foundation-tail.dross-commands
```

Compare uninterrupted and resumed final hashes.

## Suggested commits

1. `test: specify canonical save container and codecs`
2. `feat: add versioned component persistence registry`
3. `test: specify fresh-world load and migration safety`
4. `feat: add save load and resume to headless runtime`

## Exit criteria

- save is deterministic and versioned;
- load never partially mutates current world;
- one real migration fixture passes;
- current machines and RNG restore;
- map identity is verified;
- save and replay share canonical state concepts without being the same format.

## Stop conditions

- chosen container codec cannot preserve integer precision or canonical order;
- a component cannot define a stable DTO without redesigning its ownership;
- machine restore from phase 06 fails through real save data;
- fresh-world swap requires Godot ownership assumptions.
