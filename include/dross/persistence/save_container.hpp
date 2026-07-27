#pragma once

#include <dross/foundation/result.hpp>
#include <dross/foundation/version.hpp>
#include <dross/runtime/replay.hpp>

#include <cstddef>
#include <cstdint>
#include <map>
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

} // namespace dross
