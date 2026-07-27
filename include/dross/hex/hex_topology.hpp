#pragma once

#include <dross/foundation/result.hpp>
#include <dross/hex/hex_coord.hpp>
#include <dross/identity/region_id.hpp>

#include <compare>
#include <cstdint>
#include <utility>

namespace dross {

struct HexCellId {
  RegionId region;
  HexCoord coord;
  std::int32_t layer;

  [[nodiscard]] auto operator<=>(const HexCellId&) const = default;
};

struct HexPose {
  HexCellId anchor;
  HexFacing facing;

  [[nodiscard]] auto operator<=>(const HexPose&) const = default;
};

enum class EdgeKeyError : std::uint8_t {
  identical_endpoints,
};

class EdgeKey {
public:
  [[nodiscard]] static Result<EdgeKey, EdgeKeyError> between(HexCellId first, HexCellId second);

  [[nodiscard]] const HexCellId& first() const noexcept { return first_; }
  [[nodiscard]] const HexCellId& second() const noexcept { return second_; }
  [[nodiscard]] auto operator<=>(const EdgeKey&) const = default;

private:
  EdgeKey(HexCellId first, HexCellId second)
      : first_{std::move(first)}, second_{std::move(second)} {}

  HexCellId first_;
  HexCellId second_;
};

} // namespace dross
