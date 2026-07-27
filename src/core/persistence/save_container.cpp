#include <dross/persistence/save_container.hpp>

#include <dross/foundation/byte_codec.hpp>
#include <dross/generated/schema_codec.hpp>

#include <algorithm>
#include <optional>
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
  writer.write(container.header.map_id);
  writer.write_string(encode_bytes(std::as_bytes(std::span{container.header.map_hash})));

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
  const auto map_id = reader.read_content_id();
  const auto map_hash_text = reader.read_string();
  if (!magic || *magic != save_magic || !container_version || !schema_version || !engine_major ||
      !engine_minor || !engine_patch || !ticks_per_second || !current_tick || !map_id ||
      !map_hash_text) {
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
              .map_id = *map_id,
              .map_hash = map_hash,
          },
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

} // namespace dross
