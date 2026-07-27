#pragma once

#include <dross/foundation/result.hpp>

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace dross {

enum class ContentIdErrorReason : std::uint8_t {
  missing_separator,
  extra_separator,
  empty_namespace,
  empty_name,
  invalid_character,
};

struct ContentIdError {
  std::size_t position;
  ContentIdErrorReason reason;

  [[nodiscard]] constexpr bool operator==(const ContentIdError&) const = default;
};

class ContentId {
public:
  using StableHash = std::array<std::byte, 32>;

  [[nodiscard]] static Result<ContentId, ContentIdError> parse(std::string_view text);

  [[nodiscard]] std::string_view canonical() const noexcept { return canonical_; }
  [[nodiscard]] StableHash stable_hash() const noexcept;

  [[nodiscard]] bool operator==(const ContentId&) const = default;
  [[nodiscard]] std::strong_ordering operator<=>(const ContentId& other) const noexcept {
    return canonical_ <=> other.canonical_;
  }

private:
  explicit ContentId(std::string canonical) : canonical_{std::move(canonical)} {}

  std::string canonical_;
};

} // namespace dross
