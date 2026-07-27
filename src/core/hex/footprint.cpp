#include <dross/hex/footprint.hpp>

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace dross {
namespace {

[[nodiscard]] std::int32_t checked_sum(const std::int32_t first, const std::int32_t second) {
  const auto result = static_cast<std::int64_t>(first) + second;
  constexpr auto minimum = static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::min());
  constexpr auto maximum = static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max());
  if (result < minimum || result > maximum) {
    throw std::overflow_error{"expanded footprint exceeds int32 range"};
  }
  return static_cast<std::int32_t>(result);
}

} // namespace

Result<FootprintDefinition, FootprintError>
FootprintDefinition::create(FootprintId footprint_id, std::vector<HexCoord> offsets) {
  std::ranges::sort(offsets);
  if (!std::ranges::binary_search(offsets, HexCoord{.q = 0, .r = 0})) {
    return tl::unexpected{FootprintError::missing_origin};
  }
  if (std::ranges::adjacent_find(offsets) != offsets.end()) {
    return tl::unexpected{FootprintError::duplicate_offset};
  }
  return FootprintDefinition{std::move(footprint_id), std::move(offsets)};
}

std::vector<HexCoord> FootprintDefinition::rotated(const HexFacing facing) const {
  auto result = offsets_;
  const auto turns = static_cast<unsigned int>(facing) % 6U;
  for (auto& coordinate : result) {
    for (unsigned int turn = 0; turn < turns; ++turn) {
      coordinate = rotate_clockwise(coordinate);
    }
  }
  std::ranges::sort(result);
  return result;
}

std::vector<HexCellId> FootprintDefinition::expand(const HexPose& pose) const {
  std::vector<HexCellId> result;
  const auto rotated_offsets = rotated(pose.facing);
  result.reserve(rotated_offsets.size());
  for (const auto offset : rotated_offsets) {
    result.push_back(HexCellId{.region = pose.anchor.region,
                               .coord = HexCoord{.q = checked_sum(pose.anchor.coord.q, offset.q),
                                                 .r = checked_sum(pose.anchor.coord.r, offset.r)},
                               .layer = pose.anchor.layer});
  }
  std::ranges::sort(result);
  return result;
}

} // namespace dross
