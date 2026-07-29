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

constexpr std::string_view replay_magic{"dross-replay-v2"};
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

CheckpointHash canonical_map_hash(const CompiledHexMap& map) {
  ByteWriter writer;
  const auto cell_ids = map.cell_ids();
  writer.write_u64(cell_ids.size());
  for (const auto& cell_id : cell_ids) {
    write_cell(writer, cell_id);
    const auto facts = map.cell(cell_id);
    if (!facts) {
      throw std::logic_error{"compiled map omitted a listed cell"};
    }
    writer.write_u64(static_cast<std::uint64_t>(facts->surface_height.value()));
    writer.write(facts->terrain);
    writer.write_u64(facts->base_cost.value());
    writer.write_u16(static_cast<std::uint16_t>(facts->clearance));
    writer.write_u16(facts->traversable ? 1U : 0U);
    auto tags = facts->semantic_tags;
    std::ranges::sort(tags);
    writer.write_u64(tags.size());
    for (const auto& tag : tags) {
      writer.write(tag);
    }
  }
  for (const auto& first : cell_ids) {
    for (const auto& second : map.neighbors(first)) {
      if (second < first) {
        continue;
      }
      const auto edge = map.edge(first, second);
      if (!edge) {
        throw std::logic_error{"compiled map omitted a neighboring edge"};
      }
      write_cell(writer, first);
      write_cell(writer, second);
      const auto forward = edge->from_to(first);
      const auto reverse = edge->from_to(second);
      writer.write_u16(forward.traversable ? 1U : 0U);
      writer.write_u64(forward.cost.value());
      writer.write_u16(reverse.traversable ? 1U : 0U);
      writer.write_u64(reverse.cost.value());
    }
  }
  return hash_bytes(writer.bytes());
}

CanonicalCheckpoint
canonical_checkpoint(const Tick tick, const WorldStorage& world, const OccupancyIndex& occupancy,
                     const RandomHubSnapshot& random, const WorldLifecycleSnapshot lifecycle,
                     const SimulationModeSnapshot mode,
                     const std::span<const PlaceEntityEnvelope> pending_commands,
                     const CanonicalCapabilitySnapshot& capabilities) {
  std::map<CheckpointSection, CheckpointHash> sections;
  std::map<CheckpointSection, std::map<std::string, CheckpointHash>> details;

  ByteWriter clock;
  clock.write_u64(tick.value());
  sections.emplace(CheckpointSection::clock, hash_bytes(clock.bytes()));
  details[CheckpointSection::clock].emplace("current_tick", hash_bytes(clock.bytes()));

  ByteWriter identity;
  identity.write_u64(world.allocator_snapshot().next_runtime_sequence);
  ByteWriter allocator;
  allocator.write_u64(world.allocator_snapshot().next_runtime_sequence);
  details[CheckpointSection::identity].emplace("allocator", hash_bytes(allocator.bytes()));
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
    ByteWriter entity_identity;
    entity_identity.write(entity_id);
    entity_identity.write_u16(persistent.alias.has_value() ? 1U : 0U);
    if (persistent.alias) {
      entity_identity.write(persistent.alias->content_id());
    }
    details[CheckpointSection::identity].emplace("entity/" + std::to_string(entity_id.lineage()) +
                                                     "/" + std::to_string(entity_id.sequence()),
                                                 hash_bytes(entity_identity.bytes()));
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
    ByteWriter entity_pose;
    entity_pose.write(entity_id);
    entity_pose.write_u16(pose.has_value() ? 1U : 0U);
    if (pose) {
      generated::encode_hex_pose(entity_pose, *pose);
    }
    details[CheckpointSection::components].emplace("entity/" + std::to_string(entity_id.lineage()) +
                                                       "/" + std::to_string(entity_id.sequence()) +
                                                       "/dross:hex_pose",
                                                   hash_bytes(entity_pose.bytes()));
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
    ByteWriter stream_state;
    stream_state.write(stream.id.content_id());
    stream_state.write_u64(stream.seed_material.state_low);
    stream_state.write_u64(stream.seed_material.state_high);
    stream_state.write_u64(stream.seed_material.sequence_low);
    stream_state.write_u64(stream.seed_material.sequence_high);
    stream_state.write_u64(stream.state_advance_low);
    stream_state.write_u64(stream.state_advance_high);
    stream_state.write_u64(stream.call_count);
    details[CheckpointSection::random].emplace("stream/" +
                                                   std::string{stream.id.content_id().canonical()},
                                               hash_bytes(stream_state.bytes()));
  }
  sections.emplace(CheckpointSection::random, hash_bytes(random_state.bytes()));

  ByteWriter machines;
  machines.write_u16(static_cast<std::uint16_t>(lifecycle.state));
  machines.write_u16(static_cast<std::uint16_t>(mode.state));
  sections.emplace(CheckpointSection::machines, hash_bytes(machines.bytes()));
  ByteWriter lifecycle_machine;
  lifecycle_machine.write_u16(static_cast<std::uint16_t>(lifecycle.state));
  details[CheckpointSection::machines].emplace("world_lifecycle",
                                               hash_bytes(lifecycle_machine.bytes()));
  ByteWriter mode_machine;
  mode_machine.write_u16(static_cast<std::uint16_t>(mode.state));
  details[CheckpointSection::machines].emplace("simulation_mode", hash_bytes(mode_machine.bytes()));

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
    ByteWriter command_state;
    write_command(command_state, command);
    details[CheckpointSection::pending_commands].emplace(
        "command/" + std::to_string(command.metadata.id.value()),
        hash_bytes(command_state.bytes()));
  }
  sections.emplace(CheckpointSection::pending_commands, hash_bytes(pending.bytes()));

  ByteWriter capability_state;
  const auto add_capability = [&](const std::string& name, const std::span<const std::byte> bytes,
                                  const bool add_summary_detail = true) {
    const auto hash = hash_bytes(bytes);
    capability_state.write_string(name);
    write_hash(capability_state, hash);
    if (add_summary_detail) {
      details[CheckpointSection::capabilities].emplace(name, hash);
    }
  };
  if (capabilities.movement) {
    ByteWriter movement;
    encode_movement_snapshot(movement, *capabilities.movement);
    add_capability("movement", movement.bytes());
  }
  if (capabilities.combat) {
    ByteWriter combat;
    encode_combat_session_snapshot(combat, *capabilities.combat);
    add_capability("combat/session", combat.bytes());
  }
  if (capabilities.combat_actors) {
    ByteWriter actors;
    encode_ability_resolver_snapshot(actors, *capabilities.combat_actors);
    add_capability("combat/actors", actors.bytes(), false);
    for (const auto& actor : capabilities.combat_actors->actors) {
      ByteWriter actor_state;
      encode_ability_resolver_snapshot(actor_state, AbilityResolverSnapshot{.actors = {actor}});
      details[CheckpointSection::capabilities].emplace(
          "combat/actors/" + std::to_string(actor.entity.lineage()) + "/" +
              std::to_string(actor.entity.sequence()),
          hash_bytes(actor_state.bytes()));
    }
  }
  if (capabilities.door) {
    ByteWriter door;
    encode_door_snapshot(door, *capabilities.door);
    add_capability("door", door.bytes());
  }
  if (capabilities.script) {
    const auto script = encode_script_state(*capabilities.script);
    add_capability("script/state", script, false);
    for (const auto& [address, value] : capabilities.script->values()) {
      ScriptStateBag single_value;
      single_value.apply({ScriptStateWrite{.address = address, .value = value}});
      std::string scope = address.scope.kind == ScriptScopeKind::region ? "region/" : "entity/";
      scope += address.scope.region.canonical();
      if (address.scope.entity) {
        scope += "/" + std::to_string(address.scope.entity->lineage()) + "/" +
                 std::to_string(address.scope.entity->sequence());
      }
      details[CheckpointSection::capabilities].emplace(
          "script/state/" + std::string{address.module_id.canonical()} + "/" + scope + "/" +
              address.key.value(),
          hash_bytes(encode_script_state(single_value)));
    }
  }
  if (!capability_state.bytes().empty()) {
    sections.emplace(CheckpointSection::capabilities, hash_bytes(capability_state.bytes()));
  }

  ByteWriter combined;
  for (const auto& [section, hash] : sections) {
    combined.write_u16(static_cast<std::uint16_t>(section));
    write_hash(combined, hash);
  }
  return CanonicalCheckpoint{
      .tick = tick,
      .sections = std::move(sections),
      .details = std::move(details),
      .overall = hash_bytes(combined.bytes()),
  };
}

std::vector<std::byte> encode_replay(const ReplayLog& replay) {
  ByteWriter writer;
  writer.write_string(replay_magic);
  writer.write_u16(replay.header.engine_version.major);
  writer.write_u16(replay.header.engine_version.minor);
  writer.write_u16(replay.header.engine_version.patch);
  writer.write_u32(replay.header.schema_version);
  writer.write(replay.header.scenario);
  writer.write_u64(replay.header.content_manifest.size());
  for (const auto& package : replay.header.content_manifest) {
    writer.write(package.package_id);
    writer.write_u16(package.version.major);
    writer.write_u16(package.version.minor);
    writer.write_u16(package.version.patch);
    writer.write_u64(package.dependencies.size());
    for (const auto& dependency : package.dependencies) {
      writer.write(dependency);
    }
    write_hash(writer, package.content_hash);
  }
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
  writer.write_u64(replay.canonical_events.size());
  for (const auto& event : replay.canonical_events) {
    writer.write_string(event);
  }
  writer.write_u64(replay.checkpoints.size());
  for (const auto& checkpoint : replay.checkpoints) {
    writer.write_u64(checkpoint.tick.value());
    writer.write_u64(checkpoint.sections.size());
    for (const auto& [section, hash] : checkpoint.sections) {
      writer.write_u16(static_cast<std::uint16_t>(section));
      write_hash(writer, hash);
    }
    writer.write_u64(checkpoint.details.size());
    for (const auto& [section, entries] : checkpoint.details) {
      writer.write_u16(static_cast<std::uint16_t>(section));
      writer.write_u64(entries.size());
      for (const auto& [name, hash] : entries) {
        writer.write_string(name);
        write_hash(writer, hash);
      }
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
  const auto package_count = reader.read_u64();
  if (!package_count || *package_count > reader.remaining()) {
    return tl::unexpected{ReplayDecodeError::invalid_format};
  }
  ContentManifest content_manifest;
  content_manifest.reserve(static_cast<std::size_t>(*package_count));
  for (std::uint64_t package = 0; package < *package_count; ++package) {
    const auto package_id = reader.read_content_id();
    const auto major = reader.read_u16();
    const auto minor = reader.read_u16();
    const auto patch = reader.read_u16();
    const auto dependency_count = reader.read_u64();
    if (!package_id || !major || !minor || !patch || !dependency_count ||
        *dependency_count > reader.remaining()) {
      return tl::unexpected{ReplayDecodeError::invalid_format};
    }
    std::vector<ContentId> dependencies;
    dependencies.reserve(static_cast<std::size_t>(*dependency_count));
    for (std::uint64_t dependency = 0; dependency < *dependency_count; ++dependency) {
      auto dependency_id = reader.read_content_id();
      if (!dependency_id) {
        return tl::unexpected{ReplayDecodeError::invalid_format};
      }
      dependencies.push_back(*std::move(dependency_id));
    }
    const auto content_hash = read_hash(reader);
    if (!content_hash) {
      return tl::unexpected{ReplayDecodeError::invalid_format};
    }
    content_manifest.push_back(ContentPackageRecord{
        .package_id = *package_id,
        .version = {.major = *major, .minor = *minor, .patch = *patch},
        .dependencies = std::move(dependencies),
        .content_hash = *content_hash,
    });
  }
  const auto master_seed = reader.read_u64();
  const auto algorithm_version = reader.read_u32();
  const auto command_count = reader.read_u64();
  if (!magic || *magic != replay_magic || !engine_major || !engine_minor || !engine_patch ||
      !schema_version || !scenario || !master_seed || !algorithm_version || !command_count) {
    return tl::unexpected{ReplayDecodeError::invalid_format};
  }
  ReplayLog result{
      .header = ReplayHeader{.engine_version = SemanticVersion{.major = *engine_major,
                                                               .minor = *engine_minor,
                                                               .patch = *engine_patch},
                             .schema_version = *schema_version,
                             .scenario = *scenario,
                             .content_manifest = std::move(content_manifest),
                             .master_seed = MasterSeed{*master_seed},
                             .random_algorithm_version = *algorithm_version},
      .external_commands = {},
      .machine_trace = {},
      .canonical_events = {},
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
  const auto event_count = reader.read_u64();
  if (!event_count || *event_count > reader.remaining()) {
    return tl::unexpected{ReplayDecodeError::invalid_format};
  }
  result.canonical_events.reserve(static_cast<std::size_t>(*event_count));
  for (std::uint64_t index = 0; index < *event_count; ++index) {
    auto event = reader.read_string();
    if (!event) {
      return tl::unexpected{ReplayDecodeError::invalid_format};
    }
    result.canonical_events.push_back(std::move(*event));
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
    CanonicalCheckpoint checkpoint{
        .tick = Tick{*tick}, .sections = {}, .details = {}, .overall = {}};
    for (std::uint64_t section_index = 0; section_index < *section_count; ++section_index) {
      const auto section = reader.read_u16();
      const auto hash = read_hash(reader);
      if (!section || !hash ||
          *section > static_cast<std::uint16_t>(CheckpointSection::capabilities) ||
          !checkpoint.sections.emplace(static_cast<CheckpointSection>(*section), *hash).second) {
        return tl::unexpected{ReplayDecodeError::invalid_format};
      }
    }
    const auto detail_section_count = reader.read_u64();
    if (!detail_section_count) {
      return tl::unexpected{ReplayDecodeError::invalid_format};
    }
    for (std::uint64_t detail_section = 0; detail_section < *detail_section_count;
         ++detail_section) {
      const auto section = reader.read_u16();
      const auto detail_count = reader.read_u64();
      if (!section || !detail_count ||
          *section > static_cast<std::uint16_t>(CheckpointSection::capabilities)) {
        return tl::unexpected{ReplayDecodeError::invalid_format};
      }
      auto& entries = checkpoint.details[static_cast<CheckpointSection>(*section)];
      for (std::uint64_t detail = 0; detail < *detail_count; ++detail) {
        const auto name = reader.read_string();
        const auto hash = read_hash(reader);
        if (!name || !hash || !entries.emplace(*name, *hash).second) {
          return tl::unexpected{ReplayDecodeError::invalid_format};
        }
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
    auto expected_section = expected[index].sections.begin();
    auto actual_section = actual[index].sections.begin();
    while (expected_section != expected[index].sections.end() ||
           actual_section != actual[index].sections.end()) {
      const auto section = actual_section == actual[index].sections.end() ||
                                   (expected_section != expected[index].sections.end() &&
                                    expected_section->first < actual_section->first)
                               ? expected_section->first
                               : actual_section->first;
      const bool section_matches = expected_section != expected[index].sections.end() &&
                                   actual_section != actual[index].sections.end() &&
                                   expected_section->first == actual_section->first &&
                                   expected_section->second == actual_section->second;
      if (!section_matches) {
        std::optional<std::string> detail;
        const auto expected_details = expected[index].details.find(section);
        const auto actual_details = actual[index].details.find(section);
        const std::map<std::string, CheckpointHash> no_details;
        const auto& expected_entries = expected_details == expected[index].details.end()
                                           ? no_details
                                           : expected_details->second;
        const auto& actual_entries =
            actual_details == actual[index].details.end() ? no_details : actual_details->second;
        auto expected_entry = expected_entries.begin();
        auto actual_entry = actual_entries.begin();
        while (expected_entry != expected_entries.end() || actual_entry != actual_entries.end()) {
          if (actual_entry == actual_entries.end() ||
              (expected_entry != expected_entries.end() &&
               expected_entry->first < actual_entry->first)) {
            detail = expected_entry->first;
            break;
          }
          if (expected_entry == expected_entries.end() ||
              actual_entry->first < expected_entry->first) {
            detail = actual_entry->first;
            break;
          }
          if (expected_entry->second != actual_entry->second) {
            detail = expected_entry->first;
            break;
          }
          ++expected_entry;
          ++actual_entry;
        }
        return ReplayDivergence{
            .tick = expected[index].tick, .section = section, .detail = std::move(detail)};
      }
      ++expected_section;
      ++actual_section;
    }
    return ReplayDivergence{
        .tick = expected[index].tick, .section = CheckpointSection::clock, .detail = {}};
  }
  if (expected.size() != actual.size()) {
    const auto tick = count < expected.size() ? expected[count].tick : actual[count].tick;
    return ReplayDivergence{.tick = tick, .section = CheckpointSection::clock, .detail = {}};
  }
  return std::nullopt;
}

} // namespace dross
