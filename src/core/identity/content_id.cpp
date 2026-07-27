#include <dross/identity/content_id.hpp>

#include <blake3.h>

#include <algorithm>

namespace dross {
namespace {

[[nodiscard]] constexpr bool is_allowed(const unsigned char character) noexcept {
  return (character >= static_cast<unsigned char>('a') &&
          character <= static_cast<unsigned char>('z')) ||
         (character >= static_cast<unsigned char>('0') &&
          character <= static_cast<unsigned char>('9')) ||
         character == static_cast<unsigned char>('_') ||
         character == static_cast<unsigned char>('-') ||
         character == static_cast<unsigned char>('.') ||
         character == static_cast<unsigned char>('/');
}

} // namespace

Result<ContentId, ContentIdError> ContentId::parse(const std::string_view text) {
  const auto separator = text.find(':');
  if (separator == std::string_view::npos) {
    return tl::unexpected{
        ContentIdError{.position = text.size(), .reason = ContentIdErrorReason::missing_separator}};
  }
  if (separator == 0) {
    return tl::unexpected{
        ContentIdError{.position = 0, .reason = ContentIdErrorReason::empty_namespace}};
  }
  if (separator + 1 == text.size()) {
    return tl::unexpected{
        ContentIdError{.position = text.size(), .reason = ContentIdErrorReason::empty_name}};
  }

  for (std::size_t index = 0; index < text.size(); ++index) {
    if (index == separator) {
      continue;
    }
    if (text[index] == ':') {
      return tl::unexpected{
          ContentIdError{.position = index, .reason = ContentIdErrorReason::extra_separator}};
    }
    if (!is_allowed(static_cast<unsigned char>(text[index]))) {
      return tl::unexpected{
          ContentIdError{.position = index, .reason = ContentIdErrorReason::invalid_character}};
    }
  }
  return ContentId{std::string{text}};
}

ContentId::StableHash ContentId::stable_hash() const noexcept {
  StableHash result{};
  blake3_hasher hasher{};
  blake3_hasher_init(&hasher);
  blake3_hasher_update(&hasher, canonical_.data(), canonical_.size());
  blake3_hasher_finalize(&hasher, reinterpret_cast<std::uint8_t*>(result.data()), result.size());
  return result;
}

} // namespace dross
