# ADR-0019: Mods Are Trusted Executable Code

Status: Accepted

## Context

GDScript can access engine and operating-system capabilities and is not a hostile-code sandbox. Pretending capability-scoped APIs create security would be misleading.

## Decision

Treat installed GDScript mods as trusted executable code. Use narrow APIs for invariants and compatibility, not security isolation.

## Consequences

- mod installation requires an explicit trust warning;
- no sandbox is promised;
- deterministic and save compatibility rules still apply;
- a future untrusted content format would be a different system.

## Enforcement

- documentation states the trust model;
- package manifests identify executable scripts;
- no security claims are made for ScriptContext restrictions;
- hostile-code sandboxing is out of scope.
