#pragma once

#include <compare>
#include <cstdint>
#include <iosfwd>

namespace dross {

#define DROSS_DECLARE_ID(TypeName)                                                                 \
  class TypeName {                                                                                 \
  public:                                                                                          \
    explicit constexpr TypeName(const std::uint64_t value) noexcept : value_{value} {}             \
    [[nodiscard]] constexpr std::uint64_t value() const noexcept { return value_; }                \
    [[nodiscard]] constexpr auto operator<=>(const TypeName&) const = default;                     \
                                                                                                   \
  private:                                                                                         \
    std::uint64_t value_;                                                                          \
  };                                                                                               \
  std::ostream& operator<<(std::ostream& output, TypeName value)

DROSS_DECLARE_ID(EntityId);
DROSS_DECLARE_ID(WorldInstanceId);
DROSS_DECLARE_ID(CommandId);
DROSS_DECLARE_ID(CausationId);
DROSS_DECLARE_ID(CorrelationId);

#undef DROSS_DECLARE_ID

} // namespace dross
