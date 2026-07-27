#include <dross/hex/path_planner.hpp>

#include <algorithm>
#include <cstdint>
#include <map>
#include <optional>
#include <queue>
#include <tuple>
#include <utility>
#include <vector>

namespace dross {
namespace {

struct OpenNode {
  std::uint32_t estimated_cost;
  std::uint32_t actual_cost;
  HexPose pose;
  std::uint64_t insertion_sequence;
};

struct OpenNodeLater {
  [[nodiscard]] bool operator()(const OpenNode& left, const OpenNode& right) const {
    const auto left_key =
        std::tuple{left.estimated_cost, left.actual_cost, left.pose.anchor,
                   static_cast<std::uint8_t>(left.pose.facing), left.insertion_sequence};
    const auto right_key =
        std::tuple{right.estimated_cost, right.actual_cost, right.pose.anchor,
                   static_cast<std::uint8_t>(right.pose.facing), right.insertion_sequence};
    return left_key > right_key;
  }
};

struct Candidate {
  HexPose pose;
  MovementCost cost;
};

[[nodiscard]] std::vector<Candidate> candidates(const CompiledHexMap& map,
                                                const OccupancyIndex& occupancy,
                                                const FootprintDefinition& footprint,
                                                const HexPose& from, const TraversalPolicy policy,
                                                const EntityId moving_entity) {
  std::vector<HexPose> poses;
  poses.push_back(HexPose{.anchor = from.anchor, .facing = rotate_clockwise(from.facing)});
  poses.push_back(HexPose{.anchor = from.anchor, .facing = rotate_counterclockwise(from.facing)});
  for (const auto& neighbor : map.neighbors(from.anchor)) {
    poses.push_back(HexPose{.anchor = neighbor, .facing = from.facing});
  }
  std::ranges::sort(poses);

  std::vector<Candidate> result;
  for (const auto& pose : poses) {
    const auto assessment =
        assess_transition(map, occupancy, footprint, from, pose, policy, moving_entity);
    if (assessment.allowed()) {
      result.push_back(Candidate{.pose = pose, .cost = assessment.cost});
    }
  }
  return result;
}

[[nodiscard]] PlannedPath reconstruct(const HexPose& goal,
                                      const std::map<HexPose, HexPose>& predecessor,
                                      const MovementCost total_cost,
                                      const std::uint64_t occupancy_revision) {
  std::vector<HexPose> reversed{goal};
  auto current = goal;
  while (predecessor.contains(current)) {
    current = predecessor.at(current);
    reversed.push_back(current);
  }
  std::ranges::reverse(reversed);
  return PlannedPath{.poses = std::move(reversed),
                     .total_cost = total_cost,
                     .occupancy_revision = occupancy_revision};
}

} // namespace

Result<PlannedPath, PathError>
WeightedAStarPathPlanner::plan(const CompiledHexMap& map, const OccupancyIndex& occupancy,
                               const FootprintDefinition& footprint, const HexPose& start,
                               const HexPose& goal, const TraversalPolicy policy,
                               const EntityId moving_entity) const {
  if (!assess_placement(map, occupancy, footprint, start, moving_entity).allowed()) {
    return tl::unexpected{PathError::invalid_start};
  }
  if (!assess_placement(map, occupancy, footprint, goal, moving_entity).allowed()) {
    return tl::unexpected{PathError::invalid_goal};
  }
  if (start == goal) {
    return PlannedPath{.poses = {start},
                       .total_cost = MovementCost{0},
                       .occupancy_revision = occupancy.revision()};
  }

  std::priority_queue<OpenNode, std::vector<OpenNode>, OpenNodeLater> open;
  std::map<HexPose, std::uint32_t> best_cost{{start, 0}};
  std::map<HexPose, HexPose> predecessor;
  std::uint64_t insertion_sequence = 0;
  open.push(OpenNode{.estimated_cost = 0,
                     .actual_cost = 0,
                     .pose = start,
                     .insertion_sequence = insertion_sequence++});

  while (!open.empty()) {
    const auto current = open.top();
    open.pop();
    const auto known_cost = best_cost.find(current.pose);
    if (known_cost == best_cost.end() || known_cost->second != current.actual_cost) {
      continue;
    }
    if (current.pose == goal) {
      return reconstruct(goal, predecessor, MovementCost{current.actual_cost},
                         occupancy.revision());
    }

    for (const auto& candidate :
         candidates(map, occupancy, footprint, current.pose, policy, moving_entity)) {
      const auto added = MovementCost{current.actual_cost}.checked_add(candidate.cost.value());
      if (!added) {
        return tl::unexpected{PathError::cost_overflow};
      }
      const auto cost = added->value();
      const auto previous = best_cost.find(candidate.pose);
      if (previous != best_cost.end() && previous->second <= cost) {
        continue;
      }
      best_cost.insert_or_assign(candidate.pose, cost);
      predecessor.insert_or_assign(candidate.pose, current.pose);
      open.push(OpenNode{.estimated_cost = cost,
                         .actual_cost = cost,
                         .pose = candidate.pose,
                         .insertion_sequence = insertion_sequence++});
    }
  }
  return tl::unexpected{PathError::no_path};
}

} // namespace dross
