# Phase 14 Dependency Audit

Date: 2026-07-29

## Finding

Four CPM packages had no live include, link, production, or test consumer:

- nlohmann/json;
- fmt;
- spdlog;
- RapidCheck.

Their build registrations and active lock entries were removed. The first three
were also removed from the shipped native dependency notice. Their possible
future roles remain documented but deferred until a phase introduces a live
consumer.

## Validation

A new Godot-disabled build directory configured from scratch and listed only
the live native packages. That graph built successfully and passed all 183
native tests.

The normal Godot-enabled debug preset then reconfigured and rebuilt
successfully. All eight Godot integration scripts passed. Finally, the Linux
release package rebuilt, smoke-launched, and passed all five checksum
verifications with the reduced notice inventory.

This change does not alter Dross Engine's MIT license or impose Dross licensing
on games built with it.
