#include <dross/runtime/replay.hpp>

#include <blake3.h>
#include <dross/foundation/byte_codec.hpp>
#include <dross/generated/place_entity.hpp>
#include <dross/generated/schema_codec.hpp>

#include <algorithm>
#include <stdexcept>
#include <string>
#include <utility>

namespace dross {
namespace {

constexpr std::string_view replay_magic{"dross-replay-v1"};
constexpr std::uint8_t low_nibble_mask = 0x0FU;
constexpr std::uint8_t hexadecimal_alpha_offset = 10U;

CheckpointHash hash_bytes(const std::span<const std::byte> bytes) {
  CheckpointHash result{};
  blake3_hasher hasher;
  blake3_hasher_init(&hasher);
  blake3_hasher_update(&hasher, bytes.data(), bytes.size());
  blake3_hasher_finalize(&hasher, result.data(), result.size());
  return result;
}

void write_cell(ByteWriter& writer, const HexCellId& cell) {
  writer.write(cell.region.content_id());
  writer.write_u32(static_cast<std::uint32_t>(cell.coord.q));
  writer.write_u32(static_cast<std::uint32_t>(cell.coord.r));
  writer.write_u32(static_cast<std::uint32_t>(cell.layer));
}

void write_hash(ByteWriter& writer, const CheckpointHash& hash) {
  constexpr std::string_view hexadecimal{"0123456789abcdef"};
  std::string text;
  text.reserve(hash.size() * 2U);
  for (const auto value : hash) {
    text.push_back(hexadecimal[value >> 4U]);
    text.push_back(hexadecimal[value & low_nibble_mask]);
  }
  writer.write_string(text);
}

Result<CheckpointHash, ReplayDecodeError> read_hash(ByteReader& reader) {
  const auto text = reader.read_string();
  if (!text || text->size() != CheckpointHash{}.size() * 2U) {
    return tl::unexpected{ReplayDecodeError::invalid_format};
  }
  auto nibble = [](const char value) -> std::optional<std::uint8_t> {
    if (value >= '0' && value <= '9') {
      return static_cast<std::uint8_t>(value - '0');
    }
    if (value >= 'a' && value <= 'f') {
      return static_cast<std::uint8_t>(value - 'a' + hexadecimal_alpha_offset);
    }
    return std::nullopt;
  };
  CheckpointHash result{};
  for (std::size_t index = 0; index < result.size(); ++index) {
    const auto high = nibble((*text)[index * 2U]);
    const auto low = nibble((*text)[(index * 2U) + 1U]);
    if (!high || !low) {
      return tl::unexpected{ReplayDecodeError::invalid_format};
    }
    result[index] = static_cast<std::uint8_t>((*high << 4U) | *low);
  }
  return result;
}

void write_command(ByteWriter& writer, const PlaceEntityEnvelope& command) {
  writer.write_u64(command.metadata.id.value());
  writer.write_u64(command.metadata.tick.value());
  writer.write_u16(static_cast<std::uint16_t>(command.metadata.source));
  writer.write_u64(command.metadata.causation.value());
  writer.write_u64(command.metadata.correlation.value());
  placement::encode(writer, command.payload);
}

Result<PlaceEntityEnvelope, ReplayDecodeError> read_command(ByteReader& reader) {
  const auto command_id = reader.read_u64();
  const auto tick = reader.read_u64();
  const auto source = reader.read_u16();
  const auto causation = reader.read_u64();
  const auto correlation = reader.read_u64();
  if (!command_id || !tick || !source || !causation || !correlation ||
      *source > static_cast<std::uint16_t>(CommandSource::headless_test)) {
    return tl::unexpected{ReplayDecodeError::invalid_format};
  }
  const auto payload = placement::decode_place_entity(reader);
  if (!payload) {
    return tl::unexpected{ReplayDecodeError::invalid_format};
  }
  return PlaceEntityEnvelope{
      .metadata =
          CommandMetadata{
              .id = CommandId{*command_id},
              .tick = Tick{*tick},
              .source = static_cast<CommandSource>(*source),
              .causation = CausationId{*causation},
              .correlation = CorrelationId{*correlation},
          },
      .payload = *payload,
  };
}

} // namespace

CanonicalCheckpoint
canonical_checkpoint(const Tick tick, const WorldStorage& world, const OccupancyIndex& occupancy,
                     const RandomHubSnapshot& random, const WorldLifecycleSnapshot lifecycle,
                     const SimulationModeSnapshot mode,
                     const std::span<const PlaceEntityEnvelope> pending_commands) {
  std::map<CheckpointSection, CheckpointHash> sections;

  ByteWriter clock;
  clock.write_u64(tick.value());
  sections.emplace(CheckpointSection::clock, hash_bytes(clock.bytes()));

  ByteWriter identity;
  identity.write_u64(world.allocator_snapshot().next_runtime_sequence);
  const auto entity_ids = world.read().stable_entity_ids();
  identity.write_u64(entity_ids.size());
  for (const auto entity_id : entity_ids) {
    identity.write(entity_id);
    const auto found_entity = world.read().find(entity_id);
    if (!found_entity) {
      throw std::logic_error{"stable entity missing during canonical encoding"};
    }
    const auto persistent = world.read().identity(*found_entity).value();
    identity.write_u16(persistent.alias.has_value() ? 1U : 0U);
    if (persistent.alias) {
      identity.write(persistent.alias->content_id());
    }
  }
  sections.emplace(CheckpointSection::identity, hash_bytes(identity.bytes()));

  ByteWriter components;
  components.write_string("dross:hex_pose");
  for (const auto entity_id : entity_ids) {
    const auto found_entity = world.read().find(entity_id);
    if (!found_entity) {
      throw std::logic_error{"stable entity missing during component encoding"};
    }
    const auto pose = world.read().pose(*found_entity);
    components.write(entity_id);
    components.write_u16(pose.has_value() ? 1U : 0U);
    if (pose) {
      generated::encode_hex_pose(components, *pose);
    }
  }
  sections.emplace(CheckpointSection::components, hash_bytes(components.bytes()));

  ByteWriter occupied;
  const auto entries = occupancy.entries();
  occupied.write_u64(entries.size());
  for (const auto& entry : entries) {
    write_cell(occupied, entry.cell);
    occupied.write(entry.entity);
  }
  sections.emplace(CheckpointSection::occupancy, hash_bytes(occupied.bytes()));

  ByteWriter random_state;
  random_state.write_u64(random.master_seed.value());
  random_state.write_u32(random.algorithm_version);
  random_state.write_u64(random.streams.size());
  for (const auto& stream : random.streams) {
    random_state.write(stream.id.content_id());
    random_state.write_u64(stream.seed_material.state_low);
    random_state.write_u64(stream.seed_material.state_high);
    random_state.write_u64(stream.seed_material.sequence_low);
    random_state.write_u64(stream.seed_material.sequence_high);
    random_state.write_u64(stream.state_advance_low);
    random_state.write_u64(stream.state_advance_high);
    random_state.write_u64(stream.call_count);
  }
  sections.emplace(CheckpointSection::random, hash_bytes(random_state.bytes()));

  ByteWriter machines;
  machines.write_u16(static_cast<std::uint16_t>(lifecycle.state));
  machines.write_u16(static_cast<std::uint16_t>(mode.state));
  sections.emplace(CheckpointSection::machines, hash_bytes(machines.bytes()));

  auto commands =
      std::vector<PlaceEntityEnvelope>{pending_commands.begin(), pending_commands.end()};
  std::ranges::sort(commands, [](const auto& left, const auto& right) {
    return std::tie(left.metadata.tick, left.metadata.id) <
           std::tie(right.metadata.tick, right.metadata.id);
  });
  ByteWriter pending;
  pending.write_u64(commands.size());
  for (const auto& command : commands) {
    write_command(pending, command);
  }
  sections.emplace(CheckpointSection::pending_commands, hash_bytes(pending.bytes()));

  ByteWriter combined;
  for (const auto& [section, hash] : sections) {
    combined.write_u16(static_cast<std::uint16_t>(section));
    write_hash(combined, hash);
  }
  return CanonicalCheckpoint{
      .tick = tick, .sections = std::move(sections), .overall = hash_bytes(combined.bytes())};
}

std::vector<std::byte> encode_replay(const ReplayLog& replay) {
  ByteWriter writer;
  writer.write_string(replay_magic);
  writer.write_u16(replay.header.engine_version.major);
  writer.write_u16(replay.header.engine_version.minor);
  writer.write_u16(replay.header.engine_version.patch);
  writer.write_u32(replay.header.schema_version);
  writer.write(replay.header.scenario);
  writer.write(replay.header.base_package);
  writer.write_u64(replay.header.master_seed.value());
  writer.write_u32(replay.header.random_algorithm_version);
  writer.write_u64(replay.external_commands.size());
  for (const auto& command : replay.external_commands) {
    write_command(writer, command);
  }
  writer.write_u64(replay.machine_trace.size());
  for (const auto& entry : replay.machine_trace) {
    writer.write_u16(static_cast<std::uint16_t>(entry.machine));
    writer.write_u16(static_cast<std::uint16_t>(entry.source));
    writer.write_u16(static_cast<std::uint16_t>(entry.destination));
    writer.write_u16(static_cast<std::uint16_t>(entry.event));
    writer.write_u16(static_cast<std::uint16_t>(entry.outcome));
  }
  writer.write_u64(replay.checkpoints.size());
  for (const auto& checkpoint : replay.checkpoints) {
    writer.write_u64(checkpoint.tick.value());
    writer.write_u64(checkpoint.sections.size());
    for (const auto& [section, hash] : checkpoint.sections) {
      writer.write_u16(static_cast<std::uint16_t>(section));
      write_hash(writer, hash);
    }
    write_hash(writer, checkpoint.overall);
  }
  return {writer.bytes().begin(), writer.bytes().end()};
}

Result<ReplayLog, ReplayDecodeError> decode_replay(const std::span<const std::byte> bytes) {
  ByteReader reader{bytes};
  const auto magic = reader.read_string();
  const auto engine_major = reader.read_u16();
  const auto engine_minor = reader.read_u16();
  const auto engine_patch = reader.read_u16();
  const auto schema_version = reader.read_u32();
  const auto scenario = reader.read_content_id();
  const auto base_package = reader.read_content_id();
  const auto master_seed = reader.read_u64();
  const auto algorithm_version = reader.read_u32();
  const auto command_count = reader.read_u64();
  if (!magic || *magic != replay_magic || !engine_major || !engine_minor || !engine_patch ||
      !schema_version || !scenario || !base_package || !master_seed || !algorithm_version ||
      !command_count) {
    return tl::unexpected{ReplayDecodeError::invalid_format};
  }
  ReplayLog result{
      .header = ReplayHeader{.engine_version = SemanticVersion{.major = *engine_major,
                                                               .minor = *engine_minor,
                                                               .patch = *engine_patch},
                             .schema_version = *schema_version,
                             .scenario = *scenario,
                             .base_package = *base_package,
                             .master_seed = MasterSeed{*master_seed},
                             .random_algorithm_version = *algorithm_version},
      .external_commands = {},
      .machine_trace = {},
      .checkpoints = {},
  };
  for (std::uint64_t index = 0; index < *command_count; ++index) {
    auto command = read_command(reader);
    if (!command) {
      return tl::unexpected{command.error()};
    }
    result.external_commands.push_back(*std::move(command));
  }
  const auto trace_count = reader.read_u64();
  if (!trace_count) {
    return tl::unexpected{ReplayDecodeError::invalid_format};
  }
  for (std::uint64_t index = 0; index < *trace_count; ++index) {
    const auto machine = reader.read_u16();
    const auto source = reader.read_u16();
    const auto destination = reader.read_u16();
    const auto event = reader.read_u16();
    const auto outcome = reader.read_u16();
    if (!machine || !source || !destination || !event || !outcome ||
        *machine > static_cast<std::uint16_t>(MachineFamily::simulation_mode) ||
        *source > static_cast<std::uint16_t>(MachineStateId::combat) ||
        *destination > static_cast<std::uint16_t>(MachineStateId::combat) ||
        *event > static_cast<std::uint16_t>(MachineEventId::save_boundary_requested) ||
        *outcome > static_cast<std::uint16_t>(MachineEventOutcome::rejected)) {
      return tl::unexpected{ReplayDecodeError::invalid_format};
    }
    result.machine_trace.push_back(MachineTraceEntry{
        .machine = static_cast<MachineFamily>(*machine),
        .source = static_cast<MachineStateId>(*source),
        .destination = static_cast<MachineStateId>(*destination),
        .event = static_cast<MachineEventId>(*event),
        .outcome = static_cast<MachineEventOutcome>(*outcome),
    });
  }
  const auto checkpoint_count = reader.read_u64();
  if (!checkpoint_count) {
    return tl::unexpected{ReplayDecodeError::invalid_format};
  }
  for (std::uint64_t index = 0; index < *checkpoint_count; ++index) {
    const auto tick = reader.read_u64();
    const auto section_count = reader.read_u64();
    if (!tick || !section_count) {
      return tl::unexpected{ReplayDecodeError::invalid_format};
    }
    CanonicalCheckpoint checkpoint{.tick = Tick{*tick}, .sections = {}, .overall = {}};
    for (std::uint64_t section_index = 0; section_index < *section_count; ++section_index) {
      const auto section = reader.read_u16();
      const auto hash = read_hash(reader);
      if (!section || !hash ||
          *section > static_cast<std::uint16_t>(CheckpointSection::pending_commands) ||
          !checkpoint.sections.emplace(static_cast<CheckpointSection>(*section), *hash).second) {
        return tl::unexpected{ReplayDecodeError::invalid_format};
      }
    }
    const auto overall = read_hash(reader);
    if (!overall) {
      return tl::unexpected{ReplayDecodeError::invalid_format};
    }
    checkpoint.overall = *overall;
    result.checkpoints.push_back(std::move(checkpoint));
  }
  if (reader.remaining() != 0) {
    return tl::unexpected{ReplayDecodeError::invalid_format};
  }
  return result;
}

std::optional<ReplayDivergence>
first_divergence(const std::span<const CanonicalCheckpoint> expected,
                 const std::span<const CanonicalCheckpoint> actual) {
  const auto count = std::min(expected.size(), actual.size());
  for (std::size_t index = 0; index < count; ++index) {
    if (expected[index] == actual[index]) {
      continue;
    }
    for (const auto& [section, hash] : expected[index].sections) {
      const auto found = actual[index].sections.find(section);
      if (found == actual[index].sections.end() || found->second != hash) {
        return ReplayDivergence{.tick = expected[index].tick, .section = section};
      }
    }
    return ReplayDivergence{.tick = expected[index].tick, .section = CheckpointSection::clock};
  }
  if (expected.size() != actual.size()) {
    const auto tick = count < expected.size() ? expected[count].tick : actual[count].tick;
    return ReplayDivergence{.tick = tick, .section = CheckpointSection::clock};
  }
  return std::nullopt;
}

} // namespace dross
