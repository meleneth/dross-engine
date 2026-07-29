#pragma once

#include <dross/content/content_manifest.hpp>
#include <dross/foundation/version.hpp>
#include <dross/hex/occupancy.hpp>
#include <dross/random/random_hub.hpp>
#include <dross/runtime/combat_runtime.hpp>
#include <dross/runtime/command_event_kernel.hpp>
#include <dross/runtime/door_runtime.hpp>
#include <dross/runtime/script_runtime.hpp>
#include <dross/runtime/simulation_mode.hpp>
#include <dross/runtime/world_lifecycle.hpp>
#include <dross/world/world_storage.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace dross {

using CheckpointHash = std::array<std::uint8_t, 32>;

[[nodiscard]] CheckpointHash canonical_map_hash(const CompiledHexMap& map);

enum class CheckpointSection : std::uint8_t {
  clock,
  identity,
  components,
  occupancy,
  random,
  machines,
  pending_commands,
  capabilities,
};

struct CanonicalCapabilitySnapshot {
  std::optional<MovementSnapshot> movement;
  std::optional<CombatSessionSnapshot> combat;
  std::optional<AbilityResolverSnapshot> combat_actors;
  std::optional<DoorSnapshot> door;
  std::optional<ScriptStateBag> script;
};

[[nodiscard]] CheckpointHash
canonical_capability_hash(Tick tick, const CanonicalCapabilitySnapshot& capabilities);

struct CanonicalCheckpoint {
  Tick tick;
  std::map<CheckpointSection, CheckpointHash> sections;
  std::map<CheckpointSection, std::map<std::string, CheckpointHash>> details;
  CheckpointHash overall;

  [[nodiscard]] bool operator==(const CanonicalCheckpoint&) const = default;
};

[[nodiscard]] CanonicalCheckpoint
canonical_checkpoint(Tick tick, const WorldStorage& world, const OccupancyIndex& occupancy,
                     const RandomHubSnapshot& random, WorldLifecycleSnapshot lifecycle,
                     SimulationModeSnapshot mode,
                     std::span<const PlaceEntityEnvelope> pending_commands,
                     const CanonicalCapabilitySnapshot& capabilities = {});

struct ReplayHeader {
  SemanticVersion engine_version;
  std::uint32_t schema_version;
  ContentId scenario;
  ContentManifest content_manifest;
  MasterSeed master_seed;
  std::uint32_t random_algorithm_version;

  [[nodiscard]] bool operator==(const ReplayHeader&) const = default;
};

struct ReplayLog {
  ReplayHeader header;
  std::vector<PlaceEntityEnvelope> external_commands;
  std::vector<MachineTraceEntry> machine_trace;
  std::vector<std::string> canonical_events;
  std::vector<CanonicalCheckpoint> checkpoints;

  [[nodiscard]] bool operator==(const ReplayLog&) const = default;
};

enum class ReplayDecodeError : std::uint8_t {
  invalid_format,
};

[[nodiscard]] std::vector<std::byte> encode_replay(const ReplayLog& replay);
[[nodiscard]] Result<ReplayLog, ReplayDecodeError> decode_replay(std::span<const std::byte> bytes);

struct ReplayDivergence {
  Tick tick;
  CheckpointSection section;
  std::optional<std::string> detail;
};

[[nodiscard]] std::optional<ReplayDivergence>
first_divergence(std::span<const CanonicalCheckpoint> expected,
                 std::span<const CanonicalCheckpoint> actual);

struct ReplayEventDivergence {
  std::size_t index;
  std::optional<std::string> expected;
  std::optional<std::string> actual;
};

[[nodiscard]] std::optional<ReplayEventDivergence>
first_event_divergence(std::span<const std::string> expected, std::span<const std::string> actual);

} // namespace dross
