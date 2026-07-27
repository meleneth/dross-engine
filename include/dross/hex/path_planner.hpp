#pragma once

#include <dross/foundation/result.hpp>
#include <dross/hex/traversal.hpp>

#include <cstdint>
#include <vector>

namespace dross {

enum class PathError : std::uint8_t {
  invalid_start,
  invalid_goal,
  no_path,
  cost_overflow,
};

struct PlannedPath {
  std::vector<HexPose> poses;
  MovementCost total_cost;
  std::uint64_t occupancy_revision;

  [[nodiscard]] bool matches(const OccupancyIndex& occupancy) const noexcept {
    return occupancy_revision == occupancy.revision();
  }
};

class PathPlanner {
public:
  virtual ~PathPlanner() = default;

  [[nodiscard]] virtual Result<PlannedPath, PathError>
  plan(const CompiledHexMap& map, const OccupancyIndex& occupancy,
       const FootprintDefinition& footprint, const HexPose& start, const HexPose& goal,
       TraversalPolicy policy, EntityId moving_entity) const = 0;
};

class WeightedAStarPathPlanner final : public PathPlanner {
public:
  [[nodiscard]] Result<PlannedPath, PathError>
  plan(const CompiledHexMap& map, const OccupancyIndex& occupancy,
       const FootprintDefinition& footprint, const HexPose& start, const HexPose& goal,
       TraversalPolicy policy, EntityId moving_entity) const override;
};

} // namespace dross
