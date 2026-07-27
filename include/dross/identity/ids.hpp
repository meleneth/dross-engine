#pragma once

#include <compare>
#include <cstdint>
#include <iosfwd>

namespace dross {

class EntityId {
public:
  explicit constexpr EntityId(const std::uint64_t lineage, const std::uint64_t sequence) noexcept
      : lineage_{lineage}, sequence_{sequence} {}

  [[nodiscard]] constexpr std::uint64_t lineage() const noexcept { return lineage_; }
  [[nodiscard]] constexpr std::uint64_t sequence() const noexcept { return sequence_; }
  [[nodiscard]] constexpr auto operator<=>(const EntityId&) const = default;

private:
  std::uint64_t lineage_;
  std::uint64_t sequence_;
};
std::ostream& operator<<(std::ostream& output, EntityId value);

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

DROSS_DECLARE_ID(WorldInstanceId);
DROSS_DECLARE_ID(CommandId);
DROSS_DECLARE_ID(CausationId);
DROSS_DECLARE_ID(CorrelationId);

#undef DROSS_DECLARE_ID

} // namespace dross
