# Phase 00: Repository Bootstrap

## Goal

Create a reproducible, warning-clean CMake repository with pinned dependencies, local presets, GitLab CI, formatting, sanitizers, and a Godot-free test target. This phase proves the development system, not game behavior.

## Read first

- `docs/00-charter.md`
- `docs/01-architectural-invariants.md`
- `docs/03-repository-and-build.md`
- `docs/14-testing-and-quality.md`
- `docs/19-dependency-baseline.md`
- ADR-0015, ADR-0017, ADR-0018

## Preconditions

- repository is clean or newly initialized;
- current machine has a supported C++ compiler, CMake, Ninja, Git, and Python;
- network is available for first dependency acquisition, or dependency sources are already cached.

## Scope

Create only the build and quality substrate required by phase 01.

Expected initial files:

```text
CMakeLists.txt
CMakePresets.json
.gitattributes
.gitignore
.gitlab-ci.yml
cmake/CPM.cmake
cmake/Dependencies.cmake
cmake/DrossCompilerWarnings.cmake
cmake/DrossSanitizers.cmake
cmake/DrossStaticAnalysis.cmake
include/dross/foundation/version.hpp
src/core/foundation/version.cpp
tests/unit/foundation/version_test.cpp
AGENTS.md
docs/
```

Use the supplied specification documents rather than regenerating them.

## Dependency proof

Pin and configure:

- CPM.cmake;
- Catch2;
- EnTT;
- Boost.Ext.SML;
- eventpp;
- pcg-cpp;
- tl::expected;
- nlohmann/json;
- BLAKE3;
- fmt;
- spdlog;
- RapidCheck.

Dependencies that are not yet used by production code may be declared in `Dependencies.cmake`, but do not link them all into `dross_core`. Phase-specific targets link a dependency when it gains a live consumer.

Record exact versions, commit hashes, licenses, and upstream URLs in `docs/19-dependency-baseline.md` or an adjacent generated lock document.

## Build targets

Create:

```text
dross_core
dross_tests
```

`dross_core` contains one real version API so the target is not empty. `dross_tests` tests that API and proves Catch2 registration.

Do not create `dross_godot` until phase 08.

## CMake requirements

- C++20 is target-local and required;
- out-of-source builds only;
- dependencies use system include treatment;
- Dross warnings are errors;
- sanitizer presets are available on compatible compilers;
- no global warning suppression;
- no file globbing for production source lists;
- compile commands are exported for tooling;
- presets use Ninja where available;
- build output does not land inside source directories.

## Line endings

Add `.gitattributes`, run `git add --renormalize .`, and inspect the result. Source, CMake, Markdown, Python, YAML, GDScript, and Godot text assets use LF.

## Formatting and analysis

Add checked-in configurations for:

- `.clang-format`;
- `.clang-tidy` with a conservative initial rule set;
- Markdown lint only if it can be run reliably in the current environment without introducing a second JavaScript toolchain solely for this phase.

Create local scripts or CMake targets:

```text
format
format-check
tidy
```

## GitLab CI

Initial jobs:

```text
format-check
clang-debug-test
gcc-release-test
asan-ubsan-test
generated-clean, but add this job only after phase 04 creates the generator
```

Do not add fake green jobs for Windows or Godot before those targets exist. Add stages now only when a job uses them.

CI must invoke repository scripts or presets that also work locally.

## Tests first

Before implementing the version API, add a failing test requiring:

- semantic engine version structure;
- deterministic build information string excluding timestamps;
- namespace `dross`;
- no Godot dependency.

Then implement the smallest real version API.

## Architecture checks

Add a test or script proving core sources do not contain Godot includes. The first check may be a source scan, later reinforced by the CMake link graph.

## Prohibited shortcuts

- vendoring copied single headers without recording upstream and license;
- tracking dependency branches;
- weakening warnings for project code;
- leaving a dependency unpinned because it is header-only;
- creating placeholder game classes;
- adding empty targets or dead architecture registries;
- using SCons for Dross sources;
- embedding build timestamps in deterministic version output.

## Validation

Run and record exact output from the relevant local presets, for example:

```bash
cmake --preset linux-debug
cmake --build --preset linux-debug
ctest --preset linux-debug --output-on-failure
cmake --build --preset linux-debug --target format-check
cmake --preset linux-asan-ubsan
cmake --build --preset linux-asan-ubsan
ctest --preset linux-asan-ubsan --output-on-failure
```

Also run a clean configure with no Godot installation or path configured.

## Suggested commits

1. `chore: establish repository text and agent conventions`
2. `build: add pinned CMake dependency and preset foundation`
3. `test: add warning-clean core version target`
4. `ci: add GitLab core validation jobs`

Keep commits smaller when a dependency integration needs isolated review.

## Exit criteria

- clean configure and test under at least one GCC or Clang toolchain;
- sanitizer run passes;
- warnings are errors for Dross code;
- dependencies are exact and documented;
- line endings are normalized;
- CI config is syntactically valid and mirrors local commands;
- core does not require Godot;
- no unused production abstraction exists.

## Stop conditions

- RapidCheck, eventpp, SML, EnTT, pcg-cpp, or tl::expected cannot compile cleanly under the selected supported toolchain;
- godot-cpp baseline pressure appears before phase 08;
- CMake integration requires globally weakening warning or runtime settings;
- supported compilers cannot share the chosen C++20 contract.
