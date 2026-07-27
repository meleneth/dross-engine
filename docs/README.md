# Architecture Index

The documents in this directory describe one coherent engine. They are not independent idea bins.

## Foundation

- `00-charter.md`: purpose, product boundary, and success criteria.
- `01-architectural-invariants.md`: rules that every implementation must preserve.
- `02-domain-language.md`: shared vocabulary and distinctions.
- `03-repository-and-build.md`: source layout, targets, CI, and dependency discipline.
- `04-capability-architecture.md`: how a new C++ capability enters Dross.
- `05-ecs-world-model.md`: entities, components, systems, identities, and ownership.
- `06-hex-grid-model.md`: logical world geometry, footprints, edges, and path state.

## Runtime

- `07-command-event-pipeline.md`: command validation, rule contribution, event phases, and reentrancy policy.
- `08-fsm-architecture.md`: SML use, machine ownership, persistence, and author-facing deferral.
- `09-rng-fixed-tick-replay.md`: deterministic time, random streams, traces, and replay contract.
- `10-gdscript-scripting-runtime.md`: typed authoring API and script lifecycle.
- `11-godot-boundary.md`: GDExtension, views, resources, fixed-tick hosting, and presentation.
- `12-grid-authoring-editor.md`: visible grid, geometry bake, manual overrides, and diagnostics.
- `13-persistence.md`: save snapshots, component codecs, migrations, and quiescent boundaries.

## Engineering system

- `14-testing-and-quality.md`: test layers, warnings, sanitizers, and architecture checks.
- `15-code-generation-and-scaffolding.md`: Python schemas and Rails-style generators.
- `16-observability.md`: command, event, FSM, RNG, and state-hash tracing.
- `17-modding-and-versioning.md`: trusted mods, namespaces, manifests, and compatibility.
- `18-first-vertical-slice.md`: exact proof required from Thump on Field Mouse.
- `19-dependency-baseline.md`: initial pinned technologies and upgrade rules.
- `20-open-decisions.md`: explicitly deferred questions that Codex may not casually solve.

## Decision records and phases

- `adrs/`: accepted decisions and their consequences.
- `phases/`: implementation sequence and phase-specific exit criteria.
