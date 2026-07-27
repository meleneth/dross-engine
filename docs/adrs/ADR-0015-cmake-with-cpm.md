# ADR-0015: Build with CMake and CPM

Status: Accepted

## Context

Dross targets Windows, Linux, and Steam Deck with several header-only and compiled dependencies. Reproducible dependency pins and a familiar C++ build are required.

## Decision

Use CMake as the single native build system and CPM.cmake as the dependency acquisition layer. Pin every dependency to an immutable tag or commit.

## Consequences

- CMake presets become the supported local interface;
- dependency warning isolation must be configured;
- godot-cpp CMake integration is owned at the top level;
- cached sources support offline rebuilds after bootstrap.

## Enforcement

- no moving branches;
- no second SCons project for Dross sources;
- CI invokes the same presets and scripts as local development;
- dependency pins and licenses are documented.
