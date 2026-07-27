#include <dross/hex/hex_coord.hpp>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <limits>
#include <stdexcept>

namespace dross {
namespace {

constexpr std::uint8_t direction_count = 6;

[[nodiscard]] constexpr std::uint8_t normalized(const HexDirection direction) noexcept {
  return static_cast<std::uint8_t>(direction) % direction_count;
}

[[nodiscard]] constexpr std::uint8_t normalized(const HexFacing facing) noexcept {
  return static_cast<std::uint8_t>(facing) % direction_count;
}

[[nodiscard]] std::int32_t checked_i32(const std::int64_t value) {
  constexpr auto minimum = static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::min());
  constexpr auto maximum = static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max());
  if (value < minimum || value > maximum) {
    throw std::overflow_error{"hex coordinate exceeds int32 range"};
  }
  return static_cast<std::int32_t>(value);
}

} // namespace

Result<HexCoord, CubeCoordError> to_axial(const CubeCoord coordinate) {
  if (coordinate.x + coordinate.y + coordinate.z != 0) {
    return tl::unexpected{CubeCoordError::invalid_sum};
  }
  constexpr auto minimum = static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::min());
  constexpr auto maximum = static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max());
  if (coordinate.x < minimum || coordinate.x > maximum || coordinate.z < minimum ||
      coordinate.z > maximum) {
    return tl::unexpected{CubeCoordError::out_of_axial_range};
  }
  return HexCoord{.q = static_cast<std::int32_t>(coordinate.x),
                  .r = static_cast<std::int32_t>(coordinate.z)};
}

std::uint64_t hex_distance(const HexCoord first, const HexCoord second) noexcept {
  const auto first_cube = to_cube(first);
  const auto second_cube = to_cube(second);
  const auto difference_x = std::abs(first_cube.x - second_cube.x);
  const auto difference_y = std::abs(first_cube.y - second_cube.y);
  const auto difference_z = std::abs(first_cube.z - second_cube.z);
  return static_cast<std::uint64_t>(std::max({difference_x, difference_y, difference_z}));
}

HexCoord direction_offset(const HexDirection direction) noexcept {
  constexpr std::array offsets{HexCoord{.q = 1, .r = 0},  HexCoord{.q = 0, .r = 1},
                               HexCoord{.q = -1, .r = 1}, HexCoord{.q = -1, .r = 0},
                               HexCoord{.q = 0, .r = -1}, HexCoord{.q = 1, .r = -1}};
  return offsets[normalized(direction)];
}

HexCoord neighbor(const HexCoord coordinate, const HexDirection direction) {
  const auto offset = direction_offset(direction);
  return HexCoord{.q = checked_i32(static_cast<std::int64_t>(coordinate.q) + offset.q),
                  .r = checked_i32(static_cast<std::int64_t>(coordinate.r) + offset.r)};
}

HexDirection opposite(const HexDirection direction) noexcept {
  return static_cast<HexDirection>((normalized(direction) + 3U) % direction_count);
}

HexCoord rotate_clockwise(const HexCoord coordinate) {
  const auto q = static_cast<std::int64_t>(coordinate.q);
  const auto axial_r = static_cast<std::int64_t>(coordinate.r);
  return HexCoord{.q = checked_i32(-axial_r), .r = checked_i32(q + axial_r)};
}

HexCoord rotate_counterclockwise(const HexCoord coordinate) {
  const auto q = static_cast<std::int64_t>(coordinate.q);
  const auto axial_r = static_cast<std::int64_t>(coordinate.r);
  return HexCoord{.q = checked_i32(q + axial_r), .r = checked_i32(-q)};
}

HexFacing rotate_clockwise(const HexFacing facing) noexcept {
  return static_cast<HexFacing>((normalized(facing) + 1U) % direction_count);
}

HexFacing rotate_counterclockwise(const HexFacing facing) noexcept {
  return static_cast<HexFacing>((normalized(facing) + direction_count - 1U) % direction_count);
}

} // namespace dross
