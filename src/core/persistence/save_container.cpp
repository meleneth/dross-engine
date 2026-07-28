#include <dross/persistence/save_container.hpp>

#include <dross/foundation/byte_codec.hpp>
#include <dross/generated/schema_codec.hpp>

#include <algorithm>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>

namespace dross {
namespace {

constexpr std::string_view save_magic{"dross-save-v1"};
constexpr std::string_view hexadecimal{"0123456789abcdef"};
constexpr std::size_t hexadecimal_characters_per_byte = 2U;
constexpr std::uint8_t low_nibble_mask = 0x0FU;
constexpr std::uint8_t hexadecimal_alpha_offset = 10U;

Result<void, ComponentCodecError> validate_identity(const ComponentRecord& record) {
  if (record.version != 1) {
    return tl::unexpected{ComponentCodecError::unsupported_version};
  }
  ByteReader reader{record.payload};
  const auto has_alias = reader.read_u16();
  if (!has_alias || *has_alias > 1U) {
    return tl::unexpected{ComponentCodecError::invalid_payload};
  }
  if (*has_alias == 1U && !reader.read_content_id()) {
    return tl::unexpected{ComponentCodecError::invalid_payload};
  }
  if (reader.remaining() != 0) {
    return tl::unexpected{ComponentCodecError::invalid_payload};
  }
  return {};
}

Result<ComponentRecord, ComponentCodecError> migrate_identity(const ComponentRecord& record) {
  if (record.version > 1U) {
    return tl::unexpected{ComponentCodecError::unsupported_version};
  }
  if (record.version == 1U) {
    if (!validate_identity(record)) {
      return tl::unexpected{ComponentCodecError::invalid_payload};
    }
    return record;
  }
  if (!record.payload.empty()) {
    return tl::unexpected{ComponentCodecError::invalid_payload};
  }
  ByteWriter writer;
  writer.write_u16(0);
  return ComponentRecord{
      .type_id = record.type_id,
      .version = 1,
      .entity = record.entity,
      .payload = {writer.bytes().begin(), writer.bytes().end()},
  };
}

Result<void, ComponentCodecError> validate_pose(const ComponentRecord& record) {
  if (record.version != 1U) {
    return tl::unexpected{ComponentCodecError::unsupported_version};
  }
  ByteReader reader{record.payload};
  const auto pose = generated::decode_hex_pose(reader);
  if (!pose || reader.remaining() != 0) {
    return tl::unexpected{ComponentCodecError::invalid_payload};
  }
  return {};
}

Result<ComponentRecord, ComponentCodecError> migrate_pose(const ComponentRecord& record) {
  if (record.version != 1U) {
    return tl::unexpected{ComponentCodecError::unsupported_version};
  }
  if (!validate_pose(record)) {
    return tl::unexpected{ComponentCodecError::invalid_payload};
  }
  return record;
}

Result<void, ComponentCodecError> validate_generic(const ComponentRecord& record) {
  static_cast<void>(record);
  return {};
}

Result<ComponentRecord, ComponentCodecError> migrate_generic(const ComponentRecord& record) {
  return record;
}

std::string encode_bytes(const std::span<const std::byte> bytes) {
  std::string result;
  result.reserve(bytes.size() * hexadecimal_characters_per_byte);
  for (const auto value : bytes) {
    const auto byte = std::to_integer<std::uint8_t>(value);
    result.push_back(hexadecimal[byte >> 4U]);
    result.push_back(hexadecimal[byte & low_nibble_mask]);
  }
  return result;
}

std::optional<std::vector<std::byte>> decode_bytes(const std::string_view text) {
  if (text.size() % hexadecimal_characters_per_byte != 0) {
    return std::nullopt;
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
  std::vector<std::byte> result;
  result.reserve(text.size() / hexadecimal_characters_per_byte);
  for (std::size_t index = 0; index < text.size(); index += hexadecimal_characters_per_byte) {
    const auto high = nibble(text[index]);
    const auto low = nibble(text[index + 1U]);
    if (!high || !low) {
      return std::nullopt;
    }
    result.push_back(static_cast<std::byte>((*high << 4U) | *low));
  }
  return result;
}

struct EntityRecords {
  std::optional<EntityAlias> alias;
  std::optional<HexPose> pose;
  bool has_identity{false};
  bool has_pose{false};
};

Result<void, WorldLoadError> decode_identity_record(const ComponentRecord& record,
                                                    EntityRecords& entity,
                                                    std::set<ContentId>& aliases) {
  if (entity.has_identity) {
    return tl::unexpected{WorldLoadError::duplicate_component};
  }
  entity.has_identity = true;
  ByteReader reader{record.payload};
  const auto has_alias = reader.read_u16();
  if (!has_alias) {
    return tl::unexpected{WorldLoadError::component_invalid};
  }
  if (*has_alias == 0U) {
    return {};
  }
  const auto alias = reader.read_content_id();
  if (!alias) {
    return tl::unexpected{WorldLoadError::component_invalid};
  }
  if (!aliases.insert(*alias).second) {
    return tl::unexpected{WorldLoadError::duplicate_alias};
  }
  entity.alias = EntityAlias{*alias};
  return {};
}

Result<void, WorldLoadError> decode_pose_record(const ComponentRecord& record,
                                                EntityRecords& entity) {
  if (entity.has_pose) {
    return tl::unexpected{WorldLoadError::duplicate_component};
  }
  entity.has_pose = true;
  ByteReader reader{record.payload};
  const auto pose = generated::decode_hex_pose(reader);
  if (!pose) {
    return tl::unexpected{WorldLoadError::component_invalid};
  }
  entity.pose = *pose;
  return {};
}

void encode_runtime_snapshot(ByteWriter& writer, const SaveRuntimeSnapshot& snapshot) {
  writer.write_u64(snapshot.random.master_seed.value());
  writer.write_u32(snapshot.random.algorithm_version);
  writer.write_u64(snapshot.random.streams.size());
  for (const auto& stream : snapshot.random.streams) {
    writer.write(stream.id.content_id());
    writer.write_u64(stream.seed_material.state_low);
    writer.write_u64(stream.seed_material.state_high);
    writer.write_u64(stream.seed_material.sequence_low);
    writer.write_u64(stream.seed_material.sequence_high);
    writer.write_u64(stream.state_advance_low);
    writer.write_u64(stream.state_advance_high);
    writer.write_u64(stream.call_count);
  }
  writer.write_u16(static_cast<std::uint16_t>(snapshot.lifecycle.state));
  writer.write_u16(static_cast<std::uint16_t>(snapshot.mode.state));
}

Result<SaveRuntimeSnapshot, SaveDecodeError> decode_runtime_snapshot(ByteReader& reader) {
  const auto master_seed = reader.read_u64();
  const auto algorithm_version = reader.read_u32();
  const auto stream_count = reader.read_u64();
  if (!master_seed || !algorithm_version || !stream_count) {
    return tl::unexpected{SaveDecodeError::invalid_format};
  }
  RandomHubSnapshot random{
      .master_seed = MasterSeed{*master_seed},
      .algorithm_version = *algorithm_version,
      .streams = {},
  };
  random.streams.reserve(static_cast<std::size_t>(*stream_count));
  for (std::uint64_t index = 0; index < *stream_count; ++index) {
    const auto stream_id = reader.read_content_id();
    const auto state_low = reader.read_u64();
    const auto state_high = reader.read_u64();
    const auto sequence_low = reader.read_u64();
    const auto sequence_high = reader.read_u64();
    const auto advance_low = reader.read_u64();
    const auto advance_high = reader.read_u64();
    const auto call_count = reader.read_u64();
    if (!stream_id || !state_low || !state_high || !sequence_low || !sequence_high ||
        !advance_low || !advance_high || !call_count) {
      return tl::unexpected{SaveDecodeError::invalid_format};
    }
    random.streams.push_back(RandomStreamSnapshot{
        .id = RandomStreamId{*stream_id},
        .seed_material =
            RandomSeedMaterial{
                .state_low = *state_low,
                .state_high = *state_high,
                .sequence_low = *sequence_low,
                .sequence_high = *sequence_high,
            },
        .state_advance_low = *advance_low,
        .state_advance_high = *advance_high,
        .call_count = *call_count,
    });
  }
  const auto lifecycle = reader.read_u16();
  const auto mode = reader.read_u16();
  if (!lifecycle || !mode ||
      *lifecycle > static_cast<std::uint16_t>(WorldLifecycleState::faulted) ||
      *mode > static_cast<std::uint16_t>(SimulationModeState::combat)) {
    return tl::unexpected{SaveDecodeError::invalid_format};
  }
  return SaveRuntimeSnapshot{
      .random = std::move(random),
      .lifecycle = WorldLifecycleSnapshot{.state = static_cast<WorldLifecycleState>(*lifecycle)},
      .mode = SimulationModeSnapshot{.state = static_cast<SimulationModeState>(*mode)},
  };
}

} // namespace

Result<void, CodecRegistrationError>
ComponentCodecRegistry::register_codec(ComponentCodecDescriptor descriptor) {
  return register_codec(std::move(descriptor), &validate_generic, &migrate_generic);
}

Result<void, CodecRegistrationError>
ComponentCodecRegistry::register_codec(ComponentCodecDescriptor descriptor,
                                       const ValidateFunction validator,
                                       const MigrateFunction migrator) {
  const auto type_id = descriptor.type_id;
  const auto [position, inserted] = codecs_.emplace(
      type_id,
      CodecEntry{.descriptor = std::move(descriptor), .validate = validator, .migrate = migrator});
  static_cast<void>(position);
  if (!inserted) {
    return tl::unexpected{CodecRegistrationError::duplicate_type_id};
  }
  return {};
}

std::vector<ComponentCodecDescriptor> ComponentCodecRegistry::descriptors() const {
  std::vector<ComponentCodecDescriptor> result;
  result.reserve(codecs_.size());
  for (const auto& [type_id, codec] : codecs_) {
    static_cast<void>(type_id);
    result.push_back(codec.descriptor);
  }
  return result;
}

Result<ComponentRecord, ComponentCodecError>
ComponentCodecRegistry::migrate_to_current(const ComponentRecord& record) const {
  const auto found = codecs_.find(record.type_id);
  if (found == codecs_.end()) {
    return tl::unexpected{ComponentCodecError::unknown_type_id};
  }
  if (record.version > found->second.descriptor.current_version) {
    return tl::unexpected{ComponentCodecError::unsupported_version};
  }
  return found->second.migrate(record);
}

Result<void, ComponentCodecError>
ComponentCodecRegistry::validate(const ComponentRecord& record) const {
  const auto found = codecs_.find(record.type_id);
  if (found == codecs_.end()) {
    return tl::unexpected{ComponentCodecError::unknown_type_id};
  }
  return found->second.validate(record);
}

Result<void, CodecRegistrationError>
register_current_component_codecs(ComponentCodecRegistry& registry) {
  auto pose = registry.register_codec(
      ComponentCodecDescriptor{
          .type_id = ContentId::parse("dross:hex_pose").value(),
          .current_version = 1,
      },
      &validate_pose, &migrate_pose);
  if (!pose) {
    return pose;
  }
  return registry.register_codec(
      ComponentCodecDescriptor{
          .type_id = ContentId::parse("dross:persistent_identity").value(),
          .current_version = 1,
      },
      &validate_identity, &migrate_identity);
}

Result<void, ContentManifestError> validate_content_manifest(const ContentManifest& saved,
                                                             const ContentManifest& required) {
  if (saved.size() < required.size()) {
    return tl::unexpected{ContentManifestError::missing_package};
  }
  if (saved.size() > required.size()) {
    return tl::unexpected{ContentManifestError::unexpected_package};
  }
  for (std::size_t index = 0; index < required.size(); ++index) {
    if (saved[index].package_id != required[index].package_id) {
      const bool same_packages = std::ranges::all_of(required, [&saved](const auto& package) {
        return std::ranges::any_of(saved, [&package](const auto& candidate) {
          return candidate.package_id == package.package_id;
        });
      });
      return tl::unexpected{same_packages ? ContentManifestError::dependency_order_mismatch
                                          : ContentManifestError::missing_package};
    }
    if (saved[index].version != required[index].version) {
      return tl::unexpected{ContentManifestError::version_mismatch};
    }
    if (saved[index].dependencies != required[index].dependencies) {
      return tl::unexpected{ContentManifestError::dependency_order_mismatch};
    }
    if (saved[index].content_hash != required[index].content_hash) {
      return tl::unexpected{ContentManifestError::content_hash_mismatch};
    }
  }
  return {};
}

ContentManifest first_slice_content_manifest() {
  const auto package_hash = [](const std::string_view release_id) {
    const auto stable = ContentId::parse(release_id).value().stable_hash();
    CheckpointHash result{};
    std::ranges::transform(stable, result.begin(), [](const std::byte value) {
      return std::to_integer<std::uint8_t>(value);
    });
    return result;
  };
  return {
      ContentPackageRecord{
          .package_id = ContentId::parse("dross:base").value(),
          .version = {.major = 1, .minor = 0, .patch = 0},
          .dependencies = {},
          .content_hash = package_hash("dross:base_content_v1"),
      },
      ContentPackageRecord{
          .package_id = ContentId::parse("dross_demo:demo").value(),
          .version = {.major = 1, .minor = 0, .patch = 0},
          .dependencies = {ContentId::parse("dross:base").value()},
          .content_hash = package_hash("dross_demo:demo_content_v1"),
      },
  };
}

std::vector<std::byte> encode_save_container(const SaveContainer& container) {
  ByteWriter writer;
  writer.write_string(save_magic);
  writer.write_u32(container.header.container_version);
  writer.write_u32(container.header.simulation_schema_version);
  writer.write_u16(container.header.engine_version.major);
  writer.write_u16(container.header.engine_version.minor);
  writer.write_u16(container.header.engine_version.patch);
  writer.write_u32(container.header.ticks_per_second);
  writer.write_u64(container.header.current_tick.value());
  writer.write_u64(container.header.world_lineage);
  writer.write_u64(container.header.allocator.next_runtime_sequence);
  writer.write(container.header.map_id);
  writer.write_string(encode_bytes(std::as_bytes(std::span{container.header.map_hash})));
  encode_runtime_snapshot(writer, container.runtime);

  writer.write_u64(container.content_manifest.size());
  for (const auto& package : container.content_manifest) {
    writer.write(package.package_id);
    writer.write_u16(package.version.major);
    writer.write_u16(package.version.minor);
    writer.write_u16(package.version.patch);
    writer.write_u64(package.dependencies.size());
    for (const auto& dependency : package.dependencies) {
      writer.write(dependency);
    }
    writer.write_string(encode_bytes(std::as_bytes(std::span{package.content_hash})));
  }

  auto records = container.components;
  std::ranges::sort(records, [](const ComponentRecord& left, const ComponentRecord& right) {
    return std::tie(left.type_id, left.entity) < std::tie(right.type_id, right.entity);
  });
  writer.write_u64(records.size());
  for (const auto& record : records) {
    writer.write(record.type_id);
    writer.write_u32(record.version);
    writer.write(record.entity);
    writer.write_string(encode_bytes(record.payload));
  }
  return {writer.bytes().begin(), writer.bytes().end()};
}

Result<SaveContainer, SaveDecodeError>
decode_save_container(const std::span<const std::byte> bytes) {
  ByteReader reader{bytes};
  const auto magic = reader.read_string();
  const auto container_version = reader.read_u32();
  const auto schema_version = reader.read_u32();
  const auto engine_major = reader.read_u16();
  const auto engine_minor = reader.read_u16();
  const auto engine_patch = reader.read_u16();
  const auto ticks_per_second = reader.read_u32();
  const auto current_tick = reader.read_u64();
  const auto world_lineage = reader.read_u64();
  const auto next_runtime_sequence = reader.read_u64();
  const auto map_id = reader.read_content_id();
  const auto map_hash_text = reader.read_string();
  if (!magic || *magic != save_magic || !container_version || !schema_version || !engine_major ||
      !engine_minor || !engine_patch || !ticks_per_second || !current_tick || !world_lineage ||
      !next_runtime_sequence || !map_id || !map_hash_text) {
    return tl::unexpected{SaveDecodeError::invalid_format};
  }
  const auto map_hash_bytes = decode_bytes(*map_hash_text);
  if (!map_hash_bytes || map_hash_bytes->size() != CheckpointHash{}.size()) {
    return tl::unexpected{SaveDecodeError::invalid_format};
  }
  CheckpointHash map_hash{};
  std::ranges::transform(*map_hash_bytes, map_hash.begin(), [](const std::byte value) {
    return std::to_integer<std::uint8_t>(value);
  });
  auto runtime = decode_runtime_snapshot(reader);
  if (!runtime) {
    return tl::unexpected{runtime.error()};
  }

  const auto package_count = reader.read_u64();
  if (!package_count || *package_count > reader.remaining()) {
    return tl::unexpected{SaveDecodeError::invalid_format};
  }
  ContentManifest manifest;
  manifest.reserve(static_cast<std::size_t>(*package_count));
  for (std::uint64_t index = 0; index < *package_count; ++index) {
    const auto package_id = reader.read_content_id();
    const auto major = reader.read_u16();
    const auto minor = reader.read_u16();
    const auto patch = reader.read_u16();
    const auto dependency_count = reader.read_u64();
    if (!package_id || !major || !minor || !patch || !dependency_count ||
        *dependency_count > reader.remaining()) {
      return tl::unexpected{SaveDecodeError::invalid_format};
    }
    std::vector<ContentId> dependencies;
    dependencies.reserve(static_cast<std::size_t>(*dependency_count));
    for (std::uint64_t dependency = 0; dependency < *dependency_count; ++dependency) {
      auto dependency_id = reader.read_content_id();
      if (!dependency_id) {
        return tl::unexpected{SaveDecodeError::invalid_format};
      }
      dependencies.push_back(*std::move(dependency_id));
    }
    const auto hash_text = reader.read_string();
    const auto hash_bytes = hash_text ? decode_bytes(*hash_text) : std::nullopt;
    if (!hash_bytes || hash_bytes->size() != CheckpointHash{}.size()) {
      return tl::unexpected{SaveDecodeError::invalid_format};
    }
    CheckpointHash content_hash{};
    std::ranges::transform(*hash_bytes, content_hash.begin(), [](const std::byte value) {
      return std::to_integer<std::uint8_t>(value);
    });
    manifest.push_back(ContentPackageRecord{
        .package_id = *package_id,
        .version = {.major = *major, .minor = *minor, .patch = *patch},
        .dependencies = std::move(dependencies),
        .content_hash = content_hash,
    });
  }

  const auto component_count = reader.read_u64();
  if (!component_count) {
    return tl::unexpected{SaveDecodeError::invalid_format};
  }
  SaveContainer result{
      .header =
          SaveHeader{
              .container_version = *container_version,
              .simulation_schema_version = *schema_version,
              .engine_version = SemanticVersion{.major = *engine_major,
                                                .minor = *engine_minor,
                                                .patch = *engine_patch},
              .ticks_per_second = *ticks_per_second,
              .current_tick = Tick{*current_tick},
              .world_lineage = *world_lineage,
              .allocator =
                  EntityIdAllocatorSnapshot{.next_runtime_sequence = *next_runtime_sequence},
              .map_id = *map_id,
              .map_hash = map_hash,
          },
      .runtime = *std::move(runtime),
      .content_manifest = std::move(manifest),
      .components = {},
  };
  for (std::uint64_t index = 0; index < *component_count; ++index) {
    const auto type_id = reader.read_content_id();
    const auto version = reader.read_u32();
    const auto entity = reader.read_entity_id();
    const auto payload_text = reader.read_string();
    if (!type_id || !version || !entity || !payload_text) {
      return tl::unexpected{SaveDecodeError::invalid_format};
    }
    auto payload = decode_bytes(*payload_text);
    if (!payload) {
      return tl::unexpected{SaveDecodeError::invalid_format};
    }
    result.components.push_back(ComponentRecord{
        .type_id = *type_id,
        .version = *version,
        .entity = *entity,
        .payload = *std::move(payload),
    });
  }
  if (reader.remaining() != 0) {
    return tl::unexpected{SaveDecodeError::invalid_format};
  }
  std::ranges::sort(
      result.components, [](const ComponentRecord& left, const ComponentRecord& right) {
        return std::tie(left.type_id, left.entity) < std::tie(right.type_id, right.entity);
      });
  return result;
}

Result<WorldLoadPlan, WorldLoadError>
build_world_load_plan(const SaveContainer& container, const ComponentCodecRegistry& registry,
                      const ContentId& expected_map_id, const CheckpointHash& expected_map_hash) {
  if (!validate_content_manifest(container.content_manifest, first_slice_content_manifest())) {
    return tl::unexpected{WorldLoadError::content_manifest_mismatch};
  }
  if (container.header.map_id != expected_map_id ||
      container.header.map_hash != expected_map_hash) {
    return tl::unexpected{WorldLoadError::map_mismatch};
  }

  std::map<EntityId, EntityRecords> entities;
  std::set<ContentId> aliases;
  const auto identity_type = ContentId::parse("dross:persistent_identity").value();
  const auto pose_type = ContentId::parse("dross:hex_pose").value();

  for (const auto& source : container.components) {
    const auto migrated = registry.migrate_to_current(source);
    if (!migrated || !registry.validate(*migrated)) {
      return tl::unexpected{WorldLoadError::component_invalid};
    }
    if (migrated->entity.lineage() != container.header.world_lineage) {
      return tl::unexpected{WorldLoadError::wrong_lineage};
    }

    auto& entity = entities[migrated->entity];
    Result<void, WorldLoadError> decoded;
    if (migrated->type_id == identity_type) {
      decoded = decode_identity_record(*migrated, entity, aliases);
    } else if (migrated->type_id == pose_type) {
      decoded = decode_pose_record(*migrated, entity);
    }
    if (!decoded) {
      return tl::unexpected{decoded.error()};
    }
  }

  WorldLoadPlan plan;
  plan.lineage_ = container.header.world_lineage;
  plan.allocator_ = container.header.allocator;
  plan.entities_.reserve(entities.size());
  for (auto& [id, entity] : entities) {
    if (!entity.has_identity) {
      return tl::unexpected{WorldLoadError::missing_identity};
    }
    plan.entities_.push_back(PlannedEntity{
        .id = id,
        .alias = std::move(entity.alias),
        .pose = std::move(entity.pose),
    });
  }
  return plan;
}

Result<std::unique_ptr<WorldStorage>, WorldLoadError>
WorldLoadPlan::construct(const WorldInstanceId instance_id) const {
  auto world = std::make_unique<WorldStorage>(WorldConfig{
      .lineage = lineage_,
      .instance_id = instance_id,
      .next_runtime_sequence = allocator_.next_runtime_sequence,
  });
  for (const auto& entity : entities_) {
    auto spawned = world->write().spawn(SpawnPlan::authored(entity.id.sequence(), entity.alias));
    if (!spawned) {
      return tl::unexpected{WorldLoadError::construction_failed};
    }
    if (entity.pose) {
      world->write().commit_pose(*spawned, *entity.pose);
    }
  }
  return world;
}

std::vector<ComponentRecord> snapshot_world_components(const WorldStorage& world) {
  std::vector<ComponentRecord> records;
  const auto identity_type = ContentId::parse("dross:persistent_identity").value();
  const auto pose_type = ContentId::parse("dross:hex_pose").value();
  const auto read = world.read();
  for (const auto entity_id : read.stable_entity_ids()) {
    const auto entity = read.find(entity_id);
    if (!entity) {
      continue;
    }
    const auto identity = *read.identity(*entity);
    ByteWriter identity_writer;
    identity_writer.write_u16(identity.alias ? 1U : 0U);
    if (identity.alias) {
      identity_writer.write(identity.alias->content_id());
    }
    records.push_back(ComponentRecord{
        .type_id = identity_type,
        .version = 1,
        .entity = entity_id,
        .payload = {identity_writer.bytes().begin(), identity_writer.bytes().end()},
    });

    const auto pose = read.pose(*entity);
    if (pose) {
      ByteWriter pose_writer;
      generated::encode_hex_pose(pose_writer, *pose);
      records.push_back(ComponentRecord{
          .type_id = pose_type,
          .version = 1,
          .entity = entity_id,
          .payload = {pose_writer.bytes().begin(), pose_writer.bytes().end()},
      });
    }
  }
  return records;
}

} // namespace dross
