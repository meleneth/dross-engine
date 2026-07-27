#include <dross/foundation/byte_codec.hpp>

#include <limits>
#include <stdexcept>

namespace dross {
namespace {

template <class Integer>
void append_little_endian(std::vector<std::byte>& bytes, const Integer value) {
  constexpr Integer octet_mask{0xFF};
  for (std::size_t index = 0; index < sizeof(Integer); ++index) {
    const auto shift = static_cast<unsigned int>(index * 8U);
    bytes.push_back(static_cast<std::byte>((value >> shift) & octet_mask));
  }
}

template <class Integer>
Result<Integer, DecodeError> read_little_endian(std::span<const std::byte> bytes,
                                                std::size_t& position) {
  if (bytes.size() - position < sizeof(Integer)) {
    return tl::unexpected{
        DecodeError{.position = position, .reason = DecodeErrorReason::truncated}};
  }
  Integer value{0};
  for (std::size_t index = 0; index < sizeof(Integer); ++index) {
    const auto shift = static_cast<unsigned int>(index * 8U);
    const auto octet = static_cast<Integer>(std::to_integer<unsigned int>(bytes[position + index]));
    value = static_cast<Integer>(value | static_cast<Integer>(octet << shift));
  }
  position += sizeof(Integer);
  return value;
}

} // namespace

void ByteWriter::write_u16(const std::uint16_t value) { append_little_endian(bytes_, value); }

void ByteWriter::write_u32(const std::uint32_t value) { append_little_endian(bytes_, value); }

void ByteWriter::write_u64(const std::uint64_t value) { append_little_endian(bytes_, value); }

void ByteWriter::write_string(const std::string_view value) {
  if (value.size() > std::numeric_limits<std::uint32_t>::max()) {
    throw std::length_error{"byte string exceeds the canonical u32 length limit"};
  }
  write_u32(static_cast<std::uint32_t>(value.size()));
  for (const char character : value) {
    bytes_.push_back(static_cast<std::byte>(static_cast<unsigned char>(character)));
  }
}

void ByteWriter::write(const ContentId& value) { write_string(value.canonical()); }

void ByteWriter::write(const EntityId value) {
  write_u64(value.lineage());
  write_u64(value.sequence());
}

Result<std::uint16_t, DecodeError> ByteReader::read_u16() {
  return read_little_endian<std::uint16_t>(bytes_, position_);
}

Result<std::uint32_t, DecodeError> ByteReader::read_u32() {
  return read_little_endian<std::uint32_t>(bytes_, position_);
}

Result<std::uint64_t, DecodeError> ByteReader::read_u64() {
  return read_little_endian<std::uint64_t>(bytes_, position_);
}

Result<std::string, DecodeError> ByteReader::read_string() {
  const auto length = read_u32();
  if (!length) {
    return tl::unexpected{length.error()};
  }
  if (static_cast<std::uint64_t>(*length) > static_cast<std::uint64_t>(remaining())) {
    return tl::unexpected{
        DecodeError{.position = position_, .reason = DecodeErrorReason::invalid_length}};
  }
  const auto count = static_cast<std::size_t>(*length);
  const auto* data = reinterpret_cast<const char*>(bytes_.data() + position_);
  std::string result{data, count};
  position_ += count;
  return result;
}

Result<ContentId, DecodeError> ByteReader::read_content_id() {
  const auto encoded = read_string();
  if (!encoded) {
    return tl::unexpected{encoded.error()};
  }
  auto parsed = ContentId::parse(*encoded);
  if (!parsed) {
    return tl::unexpected{
        DecodeError{.position = position_ - encoded->size() + parsed.error().position,
                    .reason = DecodeErrorReason::invalid_content_id}};
  }
  return *std::move(parsed);
}

Result<EntityId, DecodeError> ByteReader::read_entity_id() {
  auto lineage = read_u64();
  if (!lineage) {
    return tl::unexpected{lineage.error()};
  }
  auto sequence = read_u64();
  if (!sequence) {
    return tl::unexpected{sequence.error()};
  }
  return EntityId{*lineage, *sequence};
}

} // namespace dross
