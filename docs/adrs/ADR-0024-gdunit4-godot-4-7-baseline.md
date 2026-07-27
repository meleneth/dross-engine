# ADR-0024: Select a GdUnit4 Baseline for Godot 4.7

Status: Accepted

## Context

Phase 08 requires a pinned GdUnit4 release compatible with the accepted Godot
4.7.1 baseline.

As of 2026-07-27, the latest GdUnit4 release is v6.1.3. The upstream
compatibility table supports v6.1.x through Godot 4.6.2. Godot 4.7 support is
listed only for the unreleased master line leading to v6.2.

Using v6.1.3 would knowingly select an unsupported pairing. Pinning a master
revision would contradict the current requirement to pin a compatible release.
A moving master branch would also violate the project's immutable dependency
policy.

The standalone Phase 08 headless smoke test remains useful and passes under the
exact Godot 4.7.1 runtime, but it is not a substitute for the required GdUnit4
bootstrap.

## Proposed choices

1. Wait for a GdUnit4 v6.2 release that explicitly supports Godot 4.7, then pin
   that release.
2. Accept an exact reviewed GdUnit4 commit from the v6.2 development line as a
   temporary immutable baseline, with a required migration to the first
   compatible v6.2 release.
3. Remove GdUnit4 from Phase 08 and accept the standalone headless runner as the
   project test boundary.

Choice 1 preserves the accepted dependency policy and is preferred unless the
release delay blocks project work. Choice 2 requires recording the exact commit
and validation evidence. Choice 3 changes the testing architecture and should
be selected only if GdUnit4 is no longer desired.

## Decision

Temporarily use the GdUnit4 v6.2 development line at exact commit
`769bf69f71c9d02a698646369eb4e4070aa3a53a`.

This commit identifies the addon as version 6.2.0, includes upstream CI coverage
for Godot 4.7, and follows the repository's owner-review policy. Vendor the
release archive shape so upstream development tests and repository-only files
do not become part of the Godot project.

Replace this pin with the first v6.2 release tag that explicitly supports Godot
4.7 after validating that release with the same Phase 08 suite.

## Enforcement

- record the selected immutable version in `docs/dependency-lock.md`;
- vendor or acquire the addon reproducibly in CI;
- run a GdUnit4 suite through Godot 4.7.1 headlessly;
- retain the standalone extension smoke test for load-failure diagnostics.
