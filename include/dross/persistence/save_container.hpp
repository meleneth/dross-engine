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

enum class CodecRegistrationError : std::uint8_t {
  duplicate_type_id,
};

class ComponentCodecRegistry {
public:
  [[nodiscard]] Result<void, CodecRegistrationError>
  register_codec(ComponentCodecDescriptor descriptor);
  [[nodiscard]] std::vector<ComponentCodecDescriptor> descriptors() const;

private:
  std::map<ContentId, ComponentCodecDescriptor> codecs_;
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

struct ComponentRecord {
  ContentId type_id;
  std::uint32_t version;
  EntityId entity;
  std::vector<std::byte> payload;

  [[nodiscard]] bool operator==(const ComponentRecord&) const = default;
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
