# ADR-0018: C++20 with a Dross Result Alias

Status: Accepted

## Context

C++20 is broadly supported across the intended compilers and godot-cpp baseline. Routine domain rejection needs a Result type, while `std::expected` requires C++23.

## Decision

Use C++20 and `tl::expected` behind `dross::Result<T, E>`. Expected failures use typed values. Exceptions remain available for exceptional construction or infrastructure faults, not ordinary command rejection.

## Consequences

- public C++ APIs do not mention `tl` directly;
- a later C++23 migration can replace the alias and adapters;
- error types must be designed rather than encoded as strings;
- compile settings remain conservative.

## Enforcement

- no custom expected implementation;
- no exceptions for routine validation or lookup misses;
- no raw bool plus out-parameter error APIs;
- the language standard is target-owned in top-level CMake.
