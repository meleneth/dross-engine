#pragma once

#include <dross/foundation/result.hpp>
#include <dross/identity/content_id.hpp>
#include <dross/identity/ids.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace dross {

enum class DecodeErrorReason : std::uint8_t {
  truncated,
  invalid_length,
  invalid_content_id,
};

struct DecodeError {
  std::size_t position;
  DecodeErrorReason reason;

  [[nodiscard]] constexpr bool operator==(const DecodeError&) const = default;
};

class ByteWriter {
public:
  void write_u16(std::uint16_t value);
  void write_u32(std::uint32_t value);
  void write_u64(std::uint64_t value);
  void write_string(std::string_view value);
  void write(const ContentId& value);
  void write(EntityId value);

  [[nodiscard]] std::span<const std::byte> bytes() const noexcept { return bytes_; }

private:
  std::vector<std::byte> bytes_;
};

class ByteReader {
public:
  explicit ByteReader(std::span<const std::byte> bytes) noexcept : bytes_{bytes} {}

  [[nodiscard]] Result<std::uint16_t, DecodeError> read_u16();
  [[nodiscard]] Result<std::uint32_t, DecodeError> read_u32();
  [[nodiscard]] Result<std::uint64_t, DecodeError> read_u64();
  [[nodiscard]] Result<std::string, DecodeError> read_string();
  [[nodiscard]] Result<ContentId, DecodeError> read_content_id();
  [[nodiscard]] Result<EntityId, DecodeError> read_entity_id();
  [[nodiscard]] std::size_t remaining() const noexcept { return bytes_.size() - position_; }

private:
  std::span<const std::byte> bytes_;
  std::size_t position_{0};
};

} // namespace dross
