# Initial Codex Instruction

Read `AGENTS.md` first, then read the architecture documents and ADRs referenced by `docs/phases/00-repository-bootstrap.md`.

Implement phase 00 completely. Use tests before implementation wherever behavior is involved. Keep commits small, coherent, and independently reviewable. After phase 00 satisfies every exit criterion, continue into phase 01 without asking for routine confirmation. Continue through later phases in numeric order while all of the following remain true:

- the implemented behavior matches the written invariants;
- tests are deterministic and passing;
- no dependency or Godot limitation contradicts an accepted ADR;
- no phase requires inventing an unresolved domain concept;
- no architecture boundary is being bypassed for speed;
- the working tree is clean at each phase boundary.

Stop and report instead of improvising when any stop condition in `AGENTS.md`, an ADR, or a phase brief is reached.

Do not implement a visually convenient shortcut that moves authoritative state into Godot nodes, GDScript member variables, animations, signals, physics, or navigation. Do not expose the EnTT registry or mutable components to GDScript. Do not replace EnTT, eventpp, Boost.Ext.SML, PCG, CPM, Catch2, or the specified testing approach with handwritten local substitutes.

At the end of each completed phase:

1. run every validation command named by that phase;
2. inspect the diff and remove dead or speculative code;
3. update architecture documentation only when behavior truly changed;
4. commit the phase in one or more small commits;
5. report the commits, tests, warnings, and any remaining risks;
6. proceed to the next phase unless something is structurally surprising.
