#pragma once

#include <dross/foundation/result.hpp>
#include <dross/foundation/version.hpp>
#include <dross/runtime/replay.hpp>
#include <dross/world/world_storage.hpp>

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace dross {

struct ComponentCodecDescriptor {
  ContentId type_id;
  std::uint32_t current_version;

  [[nodiscard]] bool operator==(const ComponentCodecDescriptor&) const = default;
};

struct ComponentRecord {
  ContentId type_id;
  std::uint32_t version;
  EntityId entity;
  std::vector<std::byte> payload;

  [[nodiscard]] bool operator==(const ComponentRecord&) const = default;
};

enum class CodecRegistrationError : std::uint8_t {
  duplicate_type_id,
};

enum class ComponentCodecError : std::uint8_t {
  unknown_type_id,
  unsupported_version,
  invalid_payload,
};

class ComponentCodecRegistry {
public:
  [[nodiscard]] Result<void, CodecRegistrationError>
  register_codec(ComponentCodecDescriptor descriptor);
  [[nodiscard]] std::vector<ComponentCodecDescriptor> descriptors() const;
  [[nodiscard]] Result<ComponentRecord, ComponentCodecError>
  migrate_to_current(const ComponentRecord& record) const;
  [[nodiscard]] Result<void, ComponentCodecError> validate(const ComponentRecord& record) const;

private:
  using ValidateFunction = Result<void, ComponentCodecError> (*)(const ComponentRecord&);
  using MigrateFunction = Result<ComponentRecord, ComponentCodecError> (*)(const ComponentRecord&);
  struct CodecEntry {
    ComponentCodecDescriptor descriptor;
    ValidateFunction validate;
    MigrateFunction migrate;
  };

  friend Result<void, CodecRegistrationError>
  register_current_component_codecs(ComponentCodecRegistry& registry);
  [[nodiscard]] Result<void, CodecRegistrationError>
  register_codec(ComponentCodecDescriptor descriptor, ValidateFunction validator,
                 MigrateFunction migrator);

  std::map<ContentId, CodecEntry> codecs_;
};

[[nodiscard]] Result<void, CodecRegistrationError>
register_current_component_codecs(ComponentCodecRegistry& registry);

struct SaveHeader {
  std::uint32_t container_version;
  std::uint32_t simulation_schema_version;
  SemanticVersion engine_version;
  std::uint32_t ticks_per_second;
  Tick current_tick;
  std::uint64_t world_lineage;
  EntityIdAllocatorSnapshot allocator;
  ContentId map_id;
  CheckpointHash map_hash;

  [[nodiscard]] bool operator==(const SaveHeader&) const = default;
};

struct SaveContainer {
  SaveHeader header;
  std::vector<ComponentRecord> components;

  [[nodiscard]] bool operator==(const SaveContainer&) const = default;
};

enum class SaveDecodeError : std::uint8_t {
  invalid_format,
};

[[nodiscard]] std::vector<std::byte> encode_save_container(const SaveContainer& container);
[[nodiscard]] Result<SaveContainer, SaveDecodeError>
decode_save_container(std::span<const std::byte> bytes);

enum class WorldLoadError : std::uint8_t {
  map_mismatch,
  component_invalid,
  duplicate_component,
  missing_identity,
  wrong_lineage,
  duplicate_alias,
  construction_failed,
};

struct PlannedEntity {
  EntityId id;
  std::optional<EntityAlias> alias;
  std::optional<HexPose> pose;
};

class WorldLoadPlan {
public:
  [[nodiscard]] Result<std::unique_ptr<WorldStorage>, WorldLoadError>
  construct(WorldInstanceId instance_id) const;
  [[nodiscard]] const std::vector<PlannedEntity>& entities() const noexcept { return entities_; }

private:
  friend Result<WorldLoadPlan, WorldLoadError> build_world_load_plan(const SaveContainer&,
                                                                     const ComponentCodecRegistry&,
                                                                     const ContentId&,
                                                                     const CheckpointHash&);

  std::uint64_t lineage_;
  EntityIdAllocatorSnapshot allocator_;
  std::vector<PlannedEntity> entities_;
};

[[nodiscard]] Result<WorldLoadPlan, WorldLoadError>
build_world_load_plan(const SaveContainer& container, const ComponentCodecRegistry& registry,
                      const ContentId& expected_map_id, const CheckpointHash& expected_map_hash);
[[nodiscard]] std::vector<ComponentRecord> snapshot_world_components(const WorldStorage& world);

} // namespace dross
