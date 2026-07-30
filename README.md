# Dross Engine

Dross is a deterministic, hex-grid-centered RPG simulation engine written in
modern C++ and presented through Godot.

Dross Engine is available under the permissive [MIT License](LICENSE). Games
and applications built with it may use their own license, including proprietary
licenses. When distributing the engine or an exported Godot runtime, retain the
applicable notices in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

Godot supplies rendering, animation, audio, UI, content import, scene authoring,
and editor integration. Dross owns authoritative world state, movement, combat,
finite state machines, event ordering, persistence, deterministic random
numbers, replay, and the API exposed to GDScript.

The first proving game slice is intentionally tiny:

- one arbitrary 3D room;
- one visible logical hex grid;
- one player actor;
- one field mouse;
- one door that is not on the required route to combat;
- real-time exploration;
- turn-based combat;
- one ability named `Thump`;
- one GDScript-authored reaction;
- save, reload, and deterministic replay.

The small surface is not permission to build disposable internals. Every implemented path must establish the architecture that later content will use.

## Build and test

The supported local baseline is CMake 3.25 or newer, Ninja, a C++20 compiler,
Python 3.11 or newer, and Godot 4.7.1. Dependencies are pinned and acquired
through CPM.

On Linux with GCC:

```sh
cmake --preset linux-debug
cmake --build --preset linux-debug
ctest --preset linux-debug --output-on-failure
```

The sanitizer preset uses Clang when configured that way:

```sh
cmake --preset linux-asan-ubsan -DCMAKE_CXX_COMPILER=clang++
cmake --build --preset linux-asan-ubsan
ctest --preset linux-asan-ubsan --output-on-failure
```

LeakSanitizer cannot run under some `ptrace`-supervised containers. In that
specific environment, use `ASAN_OPTIONS=detect_leaks=0` and record that leak
detection was not validated.

On a Windows developer prompt with Visual Studio's C++ tools and Ninja:

```powershell
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug
ctest --preset windows-msvc-debug --output-on-failure
```

The Windows presets are configured but remain unverified in the current
project environment. See the
[Phase 14 validation matrix](docs/validation/phase14-validation-matrix.md) for
the exact supported-platform evidence and exceptions.

Generated APIs must also be clean:

```sh
python3 -m venv .venv
.venv/bin/python -m pip install -r tools/dross_cli/requirements.lock
./bin/dross generate all
./bin/dross check generated
.venv/bin/python -m pytest -q tools/dross_cli/tests
cmake --build --preset linux-debug --target format-check
```

Only `generate all` and `check generated` are implemented generator commands.
Other scaffolding commands described in the design documents remain planned.

## Run the Godot project

Build `dross_godot` first, then open `godot/project.godot` in Godot 4.7.1:

```sh
cmake --build --preset linux-debug --target dross_godot
godot --editor --path godot
```

The main project scene is
`godot/demo/phase14_vertical_slice.tscn`. Run all Godot boundary tests with:

```sh
godot --headless --path godot --script res://tests/run_all.gd
```

The Phase 09 suite intentionally exercises contained script faults, so two
`SCRIPT ERROR` messages are expected before the aggregate runner reports that
all eight integration scripts passed.

To build and smoke-launch a Linux prototype outside the source tree:

```sh
GODOT_BIN=/path/to/Godot_v4.7.1-stable_linux.x86_64 \
  bin/package-linux /tmp/dross-package
```

The output directory must be absent or empty. Windows packaging and a real
Steam Deck device smoke are not currently verified.

## Author maps and typed content

The live editor workflow is:

1. Add or select a `DrossHexGridRegion3D`.
2. Set its region ID, cell radius, coordinate bounds, collision mask, and typed
   bake profile.
3. Use the Dross editor dock to bake geometry.
4. Paint explicit cell overrides where authored intent differs from geometry.
5. Rebake and confirm the override remains.
6. Compile the region; runtime movement and the visible overlay consume that
   same compiled map.

The runnable fixture is `godot/demo/phase10_room.tscn`; the editor smoke
procedure is in
[phase10-editor-smoke.md](docs/validation/phase10-editor-smoke.md).

Stable content uses typed Godot `Resource` classes such as
`DrossActorDefinition`, `DrossFootprintDefinition`,
`DrossAbilityDefinition`, and `DrossDoorDefinition`. Set their typed Inspector
properties and call their validation or compilation boundary before starting a
world. The live ability construction in
`godot/demo/phase14_vertical_slice.gd` is the smallest copyable example.
`Dictionary` payloads are not part of established command, event, or definition
APIs.

## Write GDScript behavior

GDScript contributes rules and reacts to immutable typed events. It does not
mutate ECS components. Durable values belong in `context.state`, and
authoritative randomness comes from `context.random`.

The live, tested example is
`godot/scripts/phase12_field_mouse.gd`. It contributes an ability rule, reacts
to `DrossDamageAppliedEvent` and `DrossActorKilledEvent`, writes durable state,
and uses a deterministic named random stream. Its boundary assertions are in
`godot/tests/run_phase12.gd` and `godot/tests/run_phase14.gd`.

## Record replay and inspect save failures

Record and verify the complete scenario with:

```sh
build/linux-debug/dross_headless scenario thump-on-field-mouse \
  --seed 12345 --record /tmp/thump.dross-replay
build/linux-debug/dross_headless replay --verify-checkpoints \
  /tmp/thump.dross-replay
```

`DrossWorldHost.save_integrated_state()` returns empty bytes when the world is
outside a supported save boundary.
`restore_integrated_state(bytes)` validates the complete replacement before
installing it and returns `false` on failure. Read
`get_last_load_error()` for the diagnostic; a rejected load leaves the current
authoritative state unchanged. The production usage is in
`godot/demo/phase14_vertical_slice.gd`.

## Add an authoritative capability

Use the live movement, door, or combat implementation as the C++ example. A
new capability should land as one complete production path:

1. Define stable commands, events, and rules under `schemas/`, then regenerate.
2. Add Godot-free core definitions and the single owner of any EnTT components.
3. Implement typed command planning, side-effect-free rejection, commit, and
   immutable event publication.
4. Add an SML machine only when the capability has a real lifecycle.
5. Add explicit snapshot DTOs and codecs for every persistent state value.
6. Add the narrow Godot `Resource`, command, query, and event bindings actually
   consumed by the feature.
7. Test core behavior first, then the real Godot boundary.
8. Run generation, formatting, architecture scans, native tests, replay, and
   Godot tests before committing.

Do not edit files under `generated/`. The generated command/event reference is
[command-event-api.md](generated/docs/command-event-api.md), and the ownership
contract is [Capability Architecture](docs/04-capability-architecture.md).

## Architecture and implementation corpus

1. Place `AGENTS.md`, `docs/`, and `CODEX-START.md` at the repository root.
2. Give Codex the text from `CODEX-START.md`.
3. Codex must read the charter, invariants, relevant ADRs, and the active phase before changing code.
4. Codex works through phases in numeric order.
5. It continues to the next phase after all exit criteria pass, unless a stop condition is encountered.
6. Each phase is completed through small, tested commits. A phase is not complete merely because something appears on screen.

## Reading order

1. `AGENTS.md`
2. `docs/00-charter.md`
3. `docs/01-architectural-invariants.md`
4. `docs/02-domain-language.md`
5. `docs/03-repository-and-build.md`
6. `docs/04-capability-architecture.md`
7. `docs/05-ecs-world-model.md`
8. `docs/06-hex-grid-model.md`
9. the remaining architecture documents as referenced by the active phase
10. `docs/phases/README.md`
11. the active phase brief

## Status conventions

- **Locked** means Codex may not reinterpret the decision during implementation.
- **Deferred** means the problem is acknowledged, but implementation is forbidden until a named phase or ADR.
- **Stop condition** means Codex must stop before committing an architectural substitution.
- **Production consumer** means a behavior exercised by the running slice, not merely a test fixture or unused registration.

## Project identity

```text
Engine name:          Dross Engine
Descriptive form:     Dross Engine powered by Godot
Repository name:      dross
C++ namespace:        dross
Godot class prefix:   Dross
CMake target prefix:  dross_
Python command:       ./bin/dross
Default content IDs:  dross:<name>
```

## Non-goals for the first implementation sequence

- web or mobile builds;
- networked simulation;
- multithreaded authoritative world mutation;
- hostile-code sandboxing for mods;
- a general runtime statechart language for authors;
- an entire dialogue, inventory, or quest product;
- physically continuous authoritative movement;
- Godot physics or navigation as the source of movement truth;
- ABI stability for third-party native extensions.

The documents describe later-safe seams, but phases must not create unused abstraction museums. An abstraction enters code only when its phase gives it a real caller and a real test.
