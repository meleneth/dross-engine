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
- one door installed in the dividing wall on the required route to combat;
- real-time exploration;
- turn-based combat;
- one ability named `Thump`;
- one GDScript-authored dialogue, inventory, and quest loop;
- save, reload, and deterministic replay.

The small surface is not permission to build disposable internals. Every implemented path must establish the architecture that later content will use.

## Dross and ThumpDemo

`godot/dross/` contains reusable engine-facing GDScript bases.
`godot/thump_demo/` is the small proving game: its scenes, authored content,
UI, and behavior scripts are game code rather than generic Dross
infrastructure. Godot boundary fixtures remain under `godot/tests/`.
`godot/thump_demo/ui/thump_hud.tscn` is the reusable game HUD composition: its
bottom bar reserves the player log/chat region and a combat-only turn-action
region. The playable scene feeds that log from committed game outcomes.

New ThumpDemo content uses the `thump_demo:` ContentId namespace. Generic
engine identities use `dross:`. Test fixtures may use a test-owned namespace,
but production ThumpDemo code does not use the earlier `demo:` or
`dross_demo:` identities.

ThumpDemo uses the 42-color
[LoSpec500 palette](https://lospec.com/palette-list/lospec500). Scene
materials, environment colors, UI styles, grid lines, paths, and interaction
feedback must select exact colors from that palette.

The playground also carries complete, publishable CC0 editions of Kenney's
Furniture Kit and Nature Kit under
`godot/assets/third_party/kenney/`. Each collection has its own source,
checksum, license, and inclusion notes; only its Godot-ready GLB models are
retained.

## Build and test

The supported local baseline is CMake 3.25 or newer, Ninja, a C++20 compiler,
Python 3.11 or newer, and Godot 4.7.1. Dependencies are pinned and acquired
through CPM.

For normal ThumpDemo development, launch the editor from the repository root:

```sh
./bin/dross-godot --editor
```

The launcher finds and verifies Godot 4.7.1, incrementally rebuilds the native
extension, and opens the `godot/` project. Run the demo directly by omitting
`--editor`. If Godot is installed somewhere nonstandard, set `GODOT_BIN` to
its executable. Use `--no-build` only when intentionally skipping the native
extension build.

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

From the repository root:

```sh
./bin/dross-godot --editor
```

The launcher verifies Godot 4.7.1 and incrementally builds `dross_godot`.
The main project scene is
`godot/thump_demo/scenes/phase14_vertical_slice.tscn`. Hover and click a hex,
or use the arrow keys and Enter, to preview and commit movement; the preview
shows the authoritative path cost. Press `C` for the caretaker, `D` for the
door, `S`/`L` to save/load, and use the visible combat controls after
approaching the field mouse. Run all Godot boundary tests with:

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
Steam Deck device smoke are not currently verified. A Windows host with Visual
Studio, Ninja, Git Bash, and Godot 4.7.1 can run the corresponding unverified
packaging workflow:

```sh
GODOT_BIN=/c/path/to/Godot_v4.7.1-stable_win64.exe \
  bin/package-windows /c/path/to/dross-package
```

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

The runnable fixture is `godot/thump_demo/scenes/phase10_room.tscn`; the editor smoke
procedure is in
[phase10-editor-smoke.md](docs/validation/phase10-editor-smoke.md).

Stable content uses typed Godot `Resource` classes such as
`DrossActorDefinition`, `DrossFootprintDefinition`,
`DrossAbilityDefinition`, and `DrossDoorDefinition`. Set their typed Inspector
properties and call their validation or compilation boundary before starting a
world. The live ability construction in
`godot/thump_demo/scenes/phase14_vertical_slice.gd` is the smallest copyable
example.
`Dictionary` payloads are not part of established command, event, or definition
APIs.

## Write GDScript behavior

GDScript contributes rules and reacts to immutable typed events. It does not
mutate ECS components. Durable values belong in `context.state`, and
authoritative randomness comes from `context.random`.

The production examples are deliberately short:

- `godot/thump_demo/scripts/caretaker_dialogue.gd` gates dialogue outcomes with
  inventory and quest queries, submits the typed tail hand-in, and reacts to
  immutable quest events emitted alongside the game-owned caretaker SML;
- `godot/thump_demo/scripts/field_mouse.gd` reacts to committed damage and death
  and grants the authored mouse-tail item;
- `godot/thump_demo/scripts/mouse_quest.gd` reacts to committed death and
  advances the authored quest stage.

Query authoritative inventory; do not mirror it in a node or script field:

```gdscript
if context.query.has_item(
        event.initiator_lineage,
        event.initiator_sequence,
        "thump_demo:mouse_tail",
        1):
    var count := context.query.inventory_count(
        event.initiator_lineage,
        event.initiator_sequence,
        "thump_demo:mouse_tail")
```

Submit a typed deferred inventory command:

```gdscript
context.commands.remove_item(
    event.initiator_lineage,
    event.initiator_sequence,
    "thump_demo:mouse_tail",
    1)
```

Gate caretaker behavior with authoritative quest state:

```gdscript
if (
        context.query.quest_status("thump_demo:mouse_quest") == "active"
        and context.query.quest_stage("thump_demo:mouse_quest")
            == "thump_demo:return_tail"
):
    context.commands.complete_quest(
        "thump_demo:mouse_quest", "thump_demo:return_tail")
```

React to a committed fact in a quest module:

```gdscript
func on_actor_killed(
        event: DrossActorKilledEvent, context: DrossScriptContext) -> void:
    if event.target_sequence != 2:
        return
    context.commands.advance_quest(
        "thump_demo:mouse_quest",
        "thump_demo:hunt_mouse",
        "thump_demo:return_tail")
```

These calls append commands to the callback transaction. Native inventory and
quest runtimes validate and commit them only after the callback succeeds.
Multi-command hand-ins roll back as a unit if any command rejects.

The caretaker is the game-specific FSM example. Its Boost.Ext.SML machine lives
in `src/thump_demo/fsm/caretaker_machine.*`, in namespace `thump_demo`, rather
than in generic Dross infrastructure. Entering the mouse hex triggers its
`MouseContacted` transition and starts the authoritative quest. The resulting
`QuestStarted`, `QuestAdvanced`, and `QuestCompleted` facts are dispatched to
`caretaker_dialogue.gd`, keeping known lifecycle state in C++ while leaving
authored reactions and dialogue behavior in GDScript.

### Add game content or an engine capability?

Keep a change in the game project when it uses existing authoritative shapes:
new dialogue wording and option IDs, quest-specific sequencing, item IDs,
authored conditions, presentation reactions, scenes, and UI belong under
`godot/thump_demo/` (or the equivalent folder in another game).

Add a C++ engine capability when the change needs a new authoritative storage
shape, a new component type, a new command or immutable fact schema, a new
deterministic query, a new lifecycle machine, or new save/replay data. GDScript
cannot add ECS component storage, mutate the registry, or make Godot scene
state authoritative.

The boundary assertions are in `godot/tests/run_phase12.gd` and
`godot/tests/run_phase14.gd`.

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
`godot/thump_demo/scenes/phase14_vertical_slice.gd`.

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
