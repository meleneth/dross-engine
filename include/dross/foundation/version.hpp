#pragma once

#include <cstdint>
#include <string_view>

namespace dross {

struct SemanticVersion {
  std::uint16_t major;
  std::uint16_t minor;
  std::uint16_t patch;

  [[nodiscard]] constexpr bool operator==(const SemanticVersion&) const = default;
};

[[nodiscard]] constexpr SemanticVersion engine_version() noexcept {
  return SemanticVersion{0, 1, 0};
}

[[nodiscard]] std::string_view build_information() noexcept;

} // namespace dross
