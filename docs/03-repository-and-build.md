# Repository and Build Architecture

## Repository shape

The repository is organized by stable boundary and capability, not by a single giant `managers/` folder.

```text
.
├── AGENTS.md
├── CMakeLists.txt
├── CMakePresets.json
├── .gitattributes
├── .gitignore
├── .gitlab-ci.yml
├── bin/
│   └── dross
├── cmake/
│   ├── CPM.cmake
│   ├── Dependencies.cmake
│   ├── DrossCompilerWarnings.cmake
│   ├── DrossSanitizers.cmake
│   ├── DrossStaticAnalysis.cmake
│   └── DrossGodot.cmake
├── include/dross/
│   ├── foundation/
│   ├── identity/
│   ├── world/
│   ├── hex/
│   ├── commands/
│   ├── events/
│   ├── replay/
│   ├── persistence/
│   └── capabilities/
├── src/core/
│   ├── foundation/
│   ├── identity/
│   ├── world/
│   ├── hex/
│   ├── runtime/
│   └── capabilities/
├── src/godot/
│   ├── bindings/
│   ├── resources/
│   ├── scripting/
│   ├── presentation/
│   ├── editor/
│   └── register_types.cpp
├── generated/
│   ├── include/dross/generated/
│   ├── src/
│   ├── gdscript/
│   └── docs/
├── schemas/
│   ├── commands/
│   ├── events/
│   ├── resources/
│   └── manifest.yaml
├── godot/
│   ├── project.godot
│   ├── dross.gdextension
│   ├── addons/dross_editor/
│   ├── content/dross/
│   ├── scenes/
│   ├── scripts/
│   └── tests/
├── tools/dross_cli/
├── tests/
│   ├── unit/
│   ├── property/
│   ├── integration/
│   ├── fixtures/
│   └── architecture/
└── docs/
```

The exact list grows only as phases introduce real files.

## CMake targets

Initial public build targets:

```text
dross_core          STATIC, Godot-free authoritative engine
dross_godot         SHARED, GDExtension adapter and Godot API
dross_headless      EXECUTABLE, scenario, replay, and diagnostic runner
dross_tests         EXECUTABLE, Catch2 core tests
dross_tools         umbrella for native tooling when needed
```

Python tooling is not linked into the engine.

`dross_core` exports only intentional public headers. Internal implementation headers remain under `src/core` and are not added to consumer include paths.

## Language baseline

Use C++20 for the first supported toolchain. Wrap recoverable results behind:

```cpp
namespace dross {
template<class T, class E>
using Result = tl::expected<T, E>;
}
```

This keeps public APIs independent from the chosen expected implementation and gives a direct migration path to `std::expected` when the supported compiler and standard baseline move to C++23.

Do not build a custom expected type.

## Dependency management

Use CMake with CPM. Every dependency must be pinned to an immutable tag or commit. Moving branches are forbidden.

`cmake/Dependencies.cmake` is the single source of truth for native dependency acquisition. It must:

- use `CPMAddPackage` or a documented external-tool path;
- mark third-party include directories as `SYSTEM`;
- disable dependency tests, examples, and tools unless Dross needs them;
- expose stable imported target names;
- record license and upstream URL in `docs/19-dependency-baseline.md`;
- support `CPM_SOURCE_CACHE` for offline reuse;
- never weaken Dross warnings globally to accommodate a dependency.

Godot itself is an external runtime/editor dependency. `godot-cpp` is a CMake dependency and is linked only by `dross_godot`.

## Presets

Provide named configure and build presets rather than tribal command lines:

```text
linux-debug
linux-release
linux-asan-ubsan
linux-tsan
windows-msvc-debug
windows-msvc-release
deck-release
```

The first phases may implement only the presets available on the current machine, but the schema and naming are established immediately.

A typical local flow:

```bash
cmake --preset linux-debug
cmake --build --preset linux-debug
ctest --preset linux-debug --output-on-failure
```

## Warning policy

Warnings are target-local and treated as errors for Dross code. Dependencies are system code.

At minimum, GCC and Clang builds should consider:

```text
-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion
-Wshadow -Wnon-virtual-dtor -Wold-style-cast -Wcast-align
-Woverloaded-virtual -Wnull-dereference -Wdouble-promotion -Wformat=2
```

MSVC uses `/W4`, `/permissive-`, and targeted additional warnings. Do not blindly enable warning flags unsupported by a compiler. Detection belongs in CMake helpers.

## Line endings

Create `.gitattributes` before source expansion.

```gitattributes
* text=auto
*.c text eol=lf
*.cc text eol=lf
*.cpp text eol=lf
*.h text eol=lf
*.hh text eol=lf
*.hpp text eol=lf
CMakeLists.txt text eol=lf
*.cmake text eol=lf
*.md text eol=lf
*.py text eol=lf
*.gd text eol=lf
*.tres text eol=lf
*.tscn text eol=lf
*.godot text eol=lf
*.yml text eol=lf
*.yaml text eol=lf
*.json text eol=lf
*.toml text eol=lf
*.sh text eol=lf
*.bat text eol=crlf
*.cmd text eol=crlf
```

After adding it to a repository with files, run `git add --renormalize .` and inspect the diff.

## GitLab CI

CI stages should remain legible:

```text
format
build
unit
integration
sanitize
package
```

Early phases may combine jobs, but scripts must be reusable locally. CI is not allowed to contain a second undocumented build system.

The baseline Linux image should be Debian-derived and install only declared build requirements. Cache CPM sources and build artifacts carefully, but never cache generated API output without also validating it against schemas.

Required early jobs:

- Linux Clang debug build and unit tests;
- Linux GCC release build and unit tests;
- formatting check;
- generated-code clean check;
- ASan plus UBSan test run.

Add Windows and Godot headless jobs as their phases make them meaningful. Steam Deck is validated through the Linux artifact and a documented device smoke test until a dedicated runner exists.

## Build-system tests

The test suite must prove:

- `dross_core` configures and builds without Godot;
- no core source includes Godot headers;
- generated files are up to date;
- the GDExtension lands in the Godot project path expected by `dross.gdextension`;
- debug and release artifacts do not accidentally share incompatible CRT or ABI settings on Windows;
- the top-level project owns language standard, runtime library, warnings, and sanitizer configuration.
