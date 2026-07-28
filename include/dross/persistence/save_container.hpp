#pragma once

#include <dross/content/content_manifest.hpp>
#include <dross/foundation/result.hpp>
#include <dross/foundation/version.hpp>
#include <dross/runtime/combat_runtime.hpp>
#include <dross/runtime/door_runtime.hpp>
#include <dross/runtime/movement_runtime.hpp>
#include <dross/runtime/replay.hpp>
#include <dross/runtime/script_runtime.hpp>
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

struct SaveRuntimeSnapshot {
  RandomHubSnapshot random;
  WorldLifecycleSnapshot lifecycle;
  SimulationModeSnapshot mode;

  [[nodiscard]] bool operator==(const SaveRuntimeSnapshot&) const = default;
};

struct CombatBoundarySnapshot {
  ContentId ability;
  CombatSessionSnapshot session;
  AbilityResolverSnapshot actors;

  [[nodiscard]] bool operator==(const CombatBoundarySnapshot&) const = default;
};

struct MovementBoundarySnapshot {
  EntityId actor;
  ContentId footprint;
  MovementSnapshot runtime;

  [[nodiscard]] bool operator==(const MovementBoundarySnapshot&) const = default;
};

struct DoorBoundarySnapshot {
  EntityId door;
  ContentId definition;
  std::vector<EdgeKey> edges;
  DoorSnapshot runtime;

  [[nodiscard]] bool operator==(const DoorBoundarySnapshot&) const = default;
};

struct ScriptBoundarySnapshot {
  std::vector<ScriptModule> modules;
  ScriptStateBag state;

  [[nodiscard]] bool operator==(const ScriptBoundarySnapshot& other) const {
    return modules == other.modules && state.values() == other.state.values();
  }
};

struct SaveContainer {
  SaveHeader header;
  SaveRuntimeSnapshot runtime;
  ContentManifest content_manifest;
  std::optional<CombatBoundarySnapshot> combat;
  std::optional<MovementBoundarySnapshot> movement;
  std::optional<DoorBoundarySnapshot> door;
  std::optional<ScriptBoundarySnapshot> script;
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
  content_manifest_mismatch,
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
