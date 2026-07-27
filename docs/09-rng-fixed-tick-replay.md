# RNG, Fixed Tick, and Replay

## Fixed tick

Authoritative simulation advances in integer ticks at a configured rate. The initial default is 30 ticks per second, selected as a reasonable RPG simulation cadence rather than tied to rendering or Godot physics.

```cpp
using Tick = StrongInt<std::uint64_t, TickTag>;

struct SimulationClock {
    Tick current;
    std::uint32_t ticks_per_second;
};
```

The tick rate is part of save and replay metadata. Changing it is a schema or behavior change, not a silent configuration tweak.

## Godot host timing

The Godot adapter accumulates real frame time and requests zero or more authoritative ticks. The core itself receives only a tick request, never frame delta.

The host has a maximum catch-up budget. If presentation falls behind beyond the budget, it reports a diagnostic rather than changing simulation step size.

Replay and headless tests advance ticks directly without wall-clock time.

## Random library

Use `imneme/pcg-cpp` and `pcg64`, matching the established Fairlanes pattern. All authoritative randomness is accessed through `RandomHub`.

```cpp
class RandomHub {
public:
    explicit RandomHub(MasterSeed seed);
    RandomStream& stream(RandomStreamId id);
    RandomHubSnapshot snapshot() const;
    Result<void, RandomRestoreError> restore(const RandomHubSnapshot& snapshot);
};
```

No capability constructs its own PCG engine directly.

## Named streams

At minimum, reserve distinct stable streams for:

```text
dross:combat
dross:initiative
dross:encounters
dross:loot
dross:world_scripts
```

Script scopes derive child stream IDs from the module ID and scope identity using a stable hash. Never use `std::hash`.

A fixed hash or BLAKE3-based derivation maps `(master seed, stream ID)` into the PCG state and sequence values. The derivation algorithm is versioned and tested with golden vectors.

## Sampling API

Dross owns deterministic sampling wrappers around PCG primitives:

```cpp
std::uint64_t bounded_u64(std::uint64_t upper_exclusive);
std::int64_t uniform_int(std::int64_t minimum, std::int64_t maximum);
bool chance(Ratio probability);
template<class T> void shuffle(std::span<T> values);
```

Requirements:

- use PCG's concrete bounded-random algorithm where available;
- no `std::uniform_*_distribution`;
- no `std::shuffle`;
- no floating-point probability in authoritative code;
- invalid bounds return typed errors or trigger contract checks as appropriate;
- golden-vector tests run on GCC, Clang, and MSVC.

## Presentation randomness

Visual particle variation, camera shake noise, and non-authoritative audio variation use a presentation random source in the Godot adapter. They never consume authoritative streams.

## Replay contract

Dross guarantees deterministic replay on supported targets when all of these match:

```text
engine build and simulation schema
content and mod manifest
initial canonical snapshot
master seed and random algorithm version
tick-stamped command stream
```

The result must match in:

- command acceptance or rejection;
- rejection reasons;
- domain event sequence and payloads;
- machine transition trace;
- checkpoint canonical state hashes;
- final canonical snapshot hash.

The replay does not record random outcomes. The seed and deterministic stream state are sufficient.

## Command stream

Player choices are not implied by the seed. Replay therefore records every external input command with:

- command ID;
- source;
- target tick;
- typed payload;
- correlation metadata;
- content manifest identity.

Commands generated deterministically by native or script reactions need not be stored as external inputs, but the replay trace may include them for diagnostics and must verify that they are regenerated identically.

## Canonical hashing

At configured checkpoints, serialize authoritative state in canonical order and hash it with an accepted stable hash library such as BLAKE3.

Canonical order includes:

1. engine schema and tick;
2. random streams by `RandomStreamId`;
3. regions by `RegionId`;
4. entities by `EntityId`;
5. component records by stable component type ID;
6. machine records by family and scope ID;
7. script state by scope and key.

Do not hash memory layout, padding, unordered container iteration, pointers, Godot object IDs, or raw floating-point presentation state.

## Cross-platform numeric policy

Authoritative quantities use:

- signed or unsigned fixed-width integers;
- strong units for ticks, AP, hit points, costs, and millimeters;
- rational values for probabilities and modifiers;
- explicitly specified overflow checks;
- stable endian encoding in snapshots and hashes.

Floating-point values may exist in imported definition data only if they are quantized into authoritative integer forms during content compilation. Runtime decisions do not branch on platform-sensitive physics or transcendental results.

## Replay runner

`dross_headless` supports:

```bash
dross_headless replay path/to/replay.dross-replay
dross_headless replay --verify-checkpoints path/to/replay.dross-replay
dross_headless scenario thump-on-field-mouse --seed 12345 --record out.dross-replay
```

On divergence it reports the first differing tick, command, event, machine transition, and canonical state section when possible.

## Tests

Required tests include:

- PCG golden vectors;
- stream derivation golden vectors;
- stream independence;
- snapshot and restore;
- fixed-tick command scheduling;
- same seed and commands produce same events;
- changed seed changes a known random result;
- replay from save midpoint equals uninterrupted run;
- canonical hashes are independent of entity creation storage order when persistent state is equivalent;
- no authoritative source file calls forbidden random APIs.
