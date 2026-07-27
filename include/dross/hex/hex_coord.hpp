#pragma once

#include <dross/foundation/result.hpp>

#include <compare>
#include <cstdint>

namespace dross {

struct HexCoord {
  std::int32_t q;
  std::int32_t r;

  [[nodiscard]] constexpr auto operator<=>(const HexCoord&) const = default;
};

struct CubeCoord {
  std::int64_t x;
  std::int64_t y;
  std::int64_t z;

  [[nodiscard]] constexpr auto operator<=>(const CubeCoord&) const = default;
};

enum class CubeCoordError : std::uint8_t {
  invalid_sum,
  out_of_axial_range,
};

enum class HexDirection : std::uint8_t {
  east = 0,
  southeast = 1,
  southwest = 2,
  west = 3,
  northwest = 4,
  northeast = 5,
};

enum class HexFacing : std::uint8_t {
  east = 0,
  southeast = 1,
  southwest = 2,
  west = 3,
  northwest = 4,
  northeast = 5,
};

[[nodiscard]] constexpr CubeCoord to_cube(const HexCoord coordinate) noexcept {
  const auto q = static_cast<std::int64_t>(coordinate.q);
  const auto r = static_cast<std::int64_t>(coordinate.r);
  return CubeCoord{.x = q, .y = -q - r, .z = r};
}

[[nodiscard]] Result<HexCoord, CubeCoordError> to_axial(CubeCoord coordinate);
[[nodiscard]] std::uint64_t hex_distance(HexCoord first, HexCoord second) noexcept;
[[nodiscard]] HexCoord direction_offset(HexDirection direction) noexcept;
[[nodiscard]] HexCoord neighbor(HexCoord coordinate, HexDirection direction);
[[nodiscard]] HexDirection opposite(HexDirection direction) noexcept;
[[nodiscard]] HexCoord rotate_clockwise(HexCoord coordinate);
[[nodiscard]] HexCoord rotate_counterclockwise(HexCoord coordinate);
[[nodiscard]] HexFacing rotate_clockwise(HexFacing facing) noexcept;
[[nodiscard]] HexFacing rotate_counterclockwise(HexFacing facing) noexcept;

} // namespace dross
