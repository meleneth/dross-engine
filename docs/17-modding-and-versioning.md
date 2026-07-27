# Modding and Versioning

## Trust model

Dross GDScript mods are trusted executable code. The engine does not claim to sandbox hostile scripts.

Capability-scoped APIs still matter because they preserve invariants, improve compatibility, and make authored behavior testable.

## Package identity

Every content package has a namespaced package ID and manifest.

Example:

```toml
id = "mousecult"
version = "0.1.0"
dross_api = "0.1"
dependencies = ["dross-core-content >= 0.1"]
entry_resources = ["res://mods/mousecult/content.tres"]
```

Content IDs from that package use its namespace:

```text
mousecult:sacred_mouse
mousecult:temple_region
mousecult:forbidden_thump_rule
```

The base demo content uses a separate package ID from the engine namespace when practical. `dross:` remains reserved for engine-defined primitives and examples.

## Load order

Resolve packages by:

1. dependency graph;
2. explicit declared override relationships;
3. stable package ID order for otherwise independent packages.

Filesystem enumeration order is forbidden.

Circular dependencies and undeclared overrides are errors.

## Override policy

A package may replace or extend content only through explicit manifest rules. Silent last-file-wins behavior is forbidden.

The compiler records the final source package and override chain for each content definition so editor and runtime diagnostics can explain where a value came from.

## API layers

Dross has distinct compatibility surfaces:

### Native source API

Private forks may change C++ source and recompile. No stable ABI is promised during the prototype period.

### GDScript API

Typed commands, events, queries, rule objects, resources, and script contexts form a versioned API. Breaking changes require an API version increment and migration notes.

### Content schema

Definition resources and manifests have explicit schema versions and migrations.

### Save schema

Save compatibility is separate from GDScript API compatibility and follows `docs/13-persistence.md`.

## Private-fork model

A Dross user is expected to own a private fork and may add C++ capabilities directly. Mingled source changes are normal, not an unsupported escape hatch.

The engine therefore prioritizes:

- copyable capability examples;
- generators that update registrations and tests;
- narrow internal ownership;
- readable generated code;
- accepted ADRs;
- migration guides.

It does not prioritize binary plugin ABI stability in the first milestone.

## First milestone

The first implementation builds the same API that mods will use, but external package discovery may remain limited.

From day one, implement:

- namespaced content IDs;
- module and package identity;
- deterministic ordering;
- content manifest hashes in saves and replay;
- explicit script scopes;
- trusted-code documentation.

After the first vertical slice, add:

- external package discovery;
- dependency resolution UI;
- package enable and disable workflow;
- override diagnostics;
- package-specific save migrations.

## Hot reload

Mod or script reload resets the active authoritative world at a safe boundary. Live in-place patching of component schemas, machine definitions, and script state is deferred.

## Removal and saves

A save that depends on a removed package fails with a structured missing-package error. Dross does not discard unknown entities, components, script state, or definitions to make the save appear loadable.

A package may supply an explicit removal migration that converts or removes its records with user-visible consequences.

## Version policy before 1.0

- Engine, GDScript API, content schema, save schema, RNG algorithm, and replay format versions are tracked separately.
- Breaking changes are allowed, but migrations or explicit incompatibility messages are required once fixtures exist.
- A version bump without tests is not compatibility work.
- The current build identity is included in traces, saves, and replay logs.
