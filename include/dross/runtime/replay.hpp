#pragma once

#include <dross/foundation/version.hpp>
#include <dross/hex/occupancy.hpp>
#include <dross/random/random_hub.hpp>
#include <dross/runtime/command_event_kernel.hpp>
#include <dross/world/world_storage.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <vector>

namespace dross {

using CheckpointHash = std::array<std::uint8_t, 32>;

enum class CheckpointSection : std::uint8_t {
  clock,
  identity,
  components,
  occupancy,
  random,
  pending_commands,
};

struct CanonicalCheckpoint {
  Tick tick;
  std::map<CheckpointSection, CheckpointHash> sections;
  CheckpointHash overall;

  [[nodiscard]] bool operator==(const CanonicalCheckpoint&) const = default;
};

[[nodiscard]] CanonicalCheckpoint
canonical_checkpoint(Tick tick, const WorldStorage& world, const OccupancyIndex& occupancy,
                     const RandomHubSnapshot& random,
                     std::span<const PlaceEntityEnvelope> pending_commands);

struct ReplayHeader {
  SemanticVersion engine_version;
  std::uint32_t schema_version;
  ContentId scenario;
  ContentId base_package;
  MasterSeed master_seed;
  std::uint32_t random_algorithm_version;

  [[nodiscard]] bool operator==(const ReplayHeader&) const = default;
};

struct ReplayLog {
  ReplayHeader header;
  std::vector<PlaceEntityEnvelope> external_commands;
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
};

[[nodiscard]] std::optional<ReplayDivergence>
first_divergence(std::span<const CanonicalCheckpoint> expected,
                 std::span<const CanonicalCheckpoint> actual);

} // namespace dross
