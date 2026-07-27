# Code Generation and Scaffolding

## Purpose

Stable commands, events, resources, and Godot wrappers otherwise require the same fact to be handwritten in C++, registration code, GDScript declarations, serialization, tests, and documentation. Dross uses schemas and a Python scaffolder to keep those surfaces coherent.

This is a Rails-style productivity tool, not a runtime reflection engine.

## Command line

The repository exposes one entry point:

```bash
./bin/dross --help
```

Initial planned commands:

```bash
./bin/dross generate capability doors
./bin/dross generate command doors OpenDoor actor:entity_ref door:entity_ref
./bin/dross generate event doors DoorOpened door:entity_ref actor:entity_ref
./bin/dross generate resource abilities AbilityDefinition
./bin/dross generate machine movement MovementLifecycle
./bin/dross generate script-example entity sacred_mouse
./bin/dross generate all
./bin/dross check generated
./bin/dross check architecture
```

A generator command refuses a dirty repository unless invoked with an explicit development override. It discovers the project root through the nearest parent `.git` directory.

## Python baseline

Use Python 3.11 or newer with a locked `pyproject.toml` environment.

Preferred established libraries:

- Typer for the CLI;
- Pydantic for schema models and validation;
- Jinja2 for readable templates;
- PyYAML for authored schemas;
- pytest for generator tests.

Do not implement a custom CLI parser, validation framework, or template language.

## Schema ownership

YAML schemas under `schemas/` are authoritative for stable API facts.

Example event schema:

```yaml
kind: event
namespace: dross.combat
name: DamageApplied
version: 1
id: dross:damage_applied
fields:
  - name: source
    type: entity_ref
    optional: true
  - name: target
    type: entity_ref
  - name: amount
    type: hit_points
  - name: damage_type
    type: content_id
```

The schema model defines a finite vocabulary of supported field types. Adding a new field type is a deliberate generator capability change with tests.

## Generated output

For stable commands and events, generation may produce:

```text
plain C++ payload structs
stable type IDs and registries
eventpp dispatch registration
command router registration
canonical serialization functions
Godot immutable wrapper classes
ClassDB binding code
GDScript type stubs or examples
Markdown API reference
schema fixtures and compile tests
```

Generated code is ordinary readable C++. Do not hide behavior behind opaque macro forests.

## Generated and handwritten boundary

Generated files live under `generated/` and begin with a clear generated-file notice. They are never manually edited.

Handwritten handlers, rule logic, systems, and machine actions live in capability directories and implement generated interfaces or consume generated types.

A regeneration must not overwrite handwritten semantics.

## Determinism

Generation is deterministic:

- schemas are sorted by stable IDs;
- template versions are recorded;
- line endings are LF;
- timestamps are excluded from generated source;
- maps use explicit stable ordering;
- running generation twice produces no diff.

CI runs `./bin/dross generate all` followed by `git diff --exit-code`.

## Scaffold quality

A generated capability skeleton must:

- compile;
- generate a focused failing behavioral test when the user invokes a scaffold command interactively; never mark it skipped;
- register nothing until a real command, event, or resource exists;
- contain no production TODO placeholders;
- create narrow public interfaces;
- update CMake target sources explicitly;
- update schema manifests and docs indexes;
- preserve LF line endings;
- summarize files created and next required semantic work.

For automated phase implementation, Codex should immediately complete any scaffolded behavior before committing. Empty scaffolds do not count as progress.

## Generator tests

Test the tool itself:

- project-root discovery;
- dirty-repository refusal;
- unknown generator or type errors;
- duplicate stable ID rejection;
- generated C++ compilation fixture;
- generated Godot registration fixture;
- deterministic output;
- safe rerun and idempotence;
- no overwrite of handwritten files;
- LF output;
- helpful failure messages.

## Machine scaffolding

`generate machine` creates:

- typed state and event declarations;
- SML transition table skeleton with no fake transitions;
- machine logger wiring;
- snapshot DTO;
- restore entry point;
- Catch2 test structure;
- persistence registration hook when requested;
- a copyable example document.

The author must supply actual states, guards, and actions before registration. The generator does not invent a generic boolean soup machine.

## Script examples

Because Dross is normally a private fork with mingled source changes, examples are first-class source material.

Generated examples should show:

- one typed query;
- one deferred command;
- one typed event callback;
- one declarative rule contribution;
- one state bag read and write;
- deterministic random access;
- a paired headless fake or GdUnit4 test;
- the corresponding C++ capability extension point.

Examples are compiled or parsed in CI so they cannot quietly rot.
