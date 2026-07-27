#include <dross/foundation/quantities.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <limits>
#include <type_traits>

static_assert(!std::is_convertible_v<dross::ActionPoints, dross::HitPoints>);
static_assert(!std::is_convertible_v<dross::MovementCost, dross::Millimeters>);

TEST_CASE("tick checked addition succeeds without implicit arithmetic") {
  constexpr dross::Tick tick{40};
  const auto advanced = tick.checked_add(2);

  REQUIRE(advanced);
  CHECK(advanced->value() == 42);
}

TEST_CASE("tick checked addition reports overflow") {
  constexpr dross::Tick last{std::numeric_limits<std::uint64_t>::max()};
  const auto advanced = last.checked_add(1);

  REQUIRE_FALSE(advanced);
  CHECK(advanced.error().operation == dross::ArithmeticOperation::addition);
}

TEST_CASE("signed quantities report overflow and underflow") {
  constexpr dross::HitPoints maximum{std::numeric_limits<std::int32_t>::max()};
  constexpr dross::ActionPoints minimum{std::numeric_limits<std::int32_t>::min()};

  CHECK_FALSE(maximum.checked_add(1));
  CHECK_FALSE(minimum.checked_subtract(1));
}
