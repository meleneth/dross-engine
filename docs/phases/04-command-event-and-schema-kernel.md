# Phase 04: Command, Event, Rule, and Schema Kernel

## Goal

Create the typed schema generator, single-handler command router, eventpp queued domain event transport, deterministic rule phases, and causation trace using one live movement-adjacent command.

## Read first

- `docs/07-command-event-pipeline.md`
- `docs/15-code-generation-and-scaffolding.md`
- `docs/16-observability.md`
- ADR-0003, ADR-0008

## Live production consumer

Introduce a small `PlaceEntity` or `RelocateEntityForSetup` command appropriate to the current world and hex capability. It validates a target pose, commits the pose and occupancy atomically, emits one live event, and drives one real listener that updates an inspection or derived record.

Do not create the full exploration `MoveTo` flow yet.

## Python tool bootstrap

Create:

```text
bin/dross
tools/dross_cli/pyproject.toml
tools/dross_cli/src/dross_cli/
tools/dross_cli/tests/
schemas/commands/
schemas/events/
generated/
```

Use Typer, Pydantic, Jinja2, PyYAML, and pytest with a locked environment.

Implement only generator commands needed by the live command and event plus `generate all` and `check generated`.

## Generated API

For the first command and event, generate:

- C++ payload types;
- stable type IDs;
- canonical encode and decode functions;
- command registry entry;
- eventpp registration or typed adapter;
- readable generated documentation;
- compile fixture;
- generated file notices.

Godot wrappers are added in phase 09 using the same schema, not generated as dead code now.

## Command router

Requirements:

- exactly one handler per type;
- duplicate registration failure at engine construction;
- structural payload validation;
- command metadata;
- typed rejection;
- previous result handling for duplicate command ID;
- capability-specific plan and no-fail commit;
- trace records.

## Rule pipeline

Implement one real `PlacementRuleQuery` with:

- engine invariant phase;
- hex capability phase;
- a fake script contribution port for tests, even though Godot implementation comes later;
- stable contribution ordering;
- typed rejection reasons;
- full trace.

The fake port is a real headless production port implementation used by `dross_headless` scenario configuration, not an empty interface.

## eventpp integration

Perform a focused implementation choice for typed heterogeneous events. Document the selected eventpp form and prove:

- immutable payload delivery;
- deterministic listener phase order;
- queued rather than reentrant behavior;
- follow-up command enqueued for a later phase;
- subscription lifetime safety;
- no dictionary or `void*` payload.

## Tests first

- generator schema validation;
- generator idempotence;
- generated output compile fixture;
- dirty-repository refusal for interactive scaffold command;
- duplicate command registration;
- duplicate event registration or invalid listener phase;
- accepted placement command;
- blocked placement rejection with unchanged canonical world summary;
- ordered rule contributions;
- event emitted only after commit;
- listener cannot reenter active command handling;
- follow-up command runs later;
- command and event causation trace;
- duplicate command ID policy.

## Prohibited shortcuts

- manual duplicated command structs and binding metadata;
- one universal event dictionary;
- eventpp used as a command router;
- synchronous event listener mutation that bypasses phases;
- numeric callback priorities;
- generator timestamps;
- generated code edited by hand;
- empty schemas for future RPG systems.

## Validation

```bash
./bin/dross generate all
./bin/dross check generated
pytest tools/dross_cli/tests
cmake --build --preset linux-debug
ctest --preset linux-debug --output-on-failure
dross_headless scenario command-event-kernel
```

Run generation twice and prove no diff.

## Suggested commits

1. `test: specify deterministic schema generation`
2. `tool: add Dross command and event generator`
3. `test: specify command planning and event phases`
4. `feat: add typed command router and eventpp queue`
5. `feat: exercise rule and reaction pipeline headlessly`

## Exit criteria

- one generated command and event are used by a production scenario;
- eventpp has a typed live emitter and listener;
- command rejection is state-neutral;
- follow-up commands are non-reentrant;
- trace shows rule and event causation;
- generator is deterministic and tested;
- no future event or command stubs exist.

## Stop conditions

- eventpp cannot provide typed queued delivery without unacceptable unsafe erasure;
- generator output requires opaque macro machinery;
- Python dependencies cannot be locked reproducibly;
- command commit cannot remain no-fail without a broader domain decision.
