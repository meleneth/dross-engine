# ADR-0023: Use the Godot 4.7 Native API Baseline

Status: Accepted

## Context

ADR-0017 selected Godot 4.7.1 with the tagged Godot 4.5 godot-cpp line to
favor a stable binding release. The project has chosen access to the matching
Godot 4.7 native API over compatibility with earlier Godot 4.x runtimes.

godot-cpp has not published a `godot-4.7-stable` tag. Its official repository
revision `357ad8694d49e56d38b487cdf59def8ab3037c83` contains the Godot 4.7 stable
extension API. A moving branch is not an acceptable dependency pin.

## Decision

Develop and test against Godot 4.7.1 and compile against the Godot 4.7 stable
API from the exact godot-cpp revision
`357ad8694d49e56d38b487cdf59def8ab3037c83`.

Set the extension minimum compatibility to Godot 4.7.

## Consequences

- native APIs introduced in Godot 4.6 and 4.7 are available;
- the extension no longer promises compatibility with Godot 4.5 or 4.6;
- the godot-cpp dependency is an immutable commit rather than a release tag;
- moving to a later godot-cpp revision remains an explicit dependency update.

## Enforcement

- record the full godot-cpp commit in the dependency lock and CMake;
- reject moving godot-cpp branch references;
- set `compatibility_minimum` to `4.7`;
- run headless extension tests with Godot 4.7.1.
