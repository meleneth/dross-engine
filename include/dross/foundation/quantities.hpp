#pragma once

#include <dross/foundation/result.hpp>

#include <cstdint>
#include <limits>
#include <type_traits>

namespace dross {

enum class ArithmeticOperation : std::uint8_t {
  addition,
  subtraction,
};

struct ArithmeticError {
  ArithmeticOperation operation;

  [[nodiscard]] constexpr bool operator==(const ArithmeticError&) const = default;
};

namespace detail {

template <class Representation, class Tag> class CheckedQuantity {
public:
  using representation_type = Representation;

  explicit constexpr CheckedQuantity(const Representation value) noexcept : value_{value} {}

  [[nodiscard]] constexpr Representation value() const noexcept { return value_; }
  [[nodiscard]] constexpr auto operator<=>(const CheckedQuantity&) const = default;

  [[nodiscard]] constexpr Result<CheckedQuantity, ArithmeticError>
  checked_add(const Representation amount) const noexcept {
    if constexpr (std::is_unsigned_v<Representation>) {
      if (amount > std::numeric_limits<Representation>::max() - value_) {
        return tl::unexpected{ArithmeticError{ArithmeticOperation::addition}};
      }
    } else {
      if ((amount > 0 && value_ > std::numeric_limits<Representation>::max() - amount) ||
          (amount < 0 && value_ < std::numeric_limits<Representation>::min() - amount)) {
        return tl::unexpected{ArithmeticError{ArithmeticOperation::addition}};
      }
    }
    return CheckedQuantity{static_cast<Representation>(value_ + amount)};
  }

  [[nodiscard]] constexpr Result<CheckedQuantity, ArithmeticError>
  checked_subtract(const Representation amount) const noexcept {
    if constexpr (std::is_unsigned_v<Representation>) {
      if (amount > value_) {
        return tl::unexpected{ArithmeticError{ArithmeticOperation::subtraction}};
      }
    } else {
      if ((amount > 0 && value_ < std::numeric_limits<Representation>::min() + amount) ||
          (amount < 0 && value_ > std::numeric_limits<Representation>::max() + amount)) {
        return tl::unexpected{ArithmeticError{ArithmeticOperation::subtraction}};
      }
    }
    return CheckedQuantity{static_cast<Representation>(value_ - amount)};
  }

private:
  Representation value_;
};

} // namespace detail

struct TickTag;
struct MillimetersTag;
struct MovementCostTag;
struct ActionPointsTag;
struct HitPointsTag;
struct InitiativeTag;

using Tick = detail::CheckedQuantity<std::uint64_t, TickTag>;
using Millimeters = detail::CheckedQuantity<std::int64_t, MillimetersTag>;
using MovementCost = detail::CheckedQuantity<std::uint32_t, MovementCostTag>;
using ActionPoints = detail::CheckedQuantity<std::int32_t, ActionPointsTag>;
using HitPoints = detail::CheckedQuantity<std::int32_t, HitPointsTag>;
using Initiative = detail::CheckedQuantity<std::int32_t, InitiativeTag>;

} // namespace dross
