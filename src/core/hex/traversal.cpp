#include <dross/hex/traversal.hpp>

#include <algorithm>

namespace dross {
namespace {

[[nodiscard]] TraversalAssessment blocked(const TraversalBlockReason reason) {
  return TraversalAssessment{.reason = reason, .cost = MovementCost{0}};
}

} // namespace

TraversalAssessment assess_placement(const CompiledHexMap& map, const OccupancyIndex& occupancy,
                                     const FootprintDefinition& footprint, const HexPose& pose,
                                     const EntityId moving_entity) {
  const auto occupied_cells = footprint.expand(pose);
  for (const auto& cell_id : occupied_cells) {
    const auto facts = map.cell(cell_id);
    if (!facts) {
      return blocked(TraversalBlockReason::missing_cell);
    }
    if (!facts->traversable || facts->clearance == Clearance::blocked) {
      return blocked(TraversalBlockReason::blocked_cell);
    }
  }
  if (!occupancy.can_occupy(occupied_cells, moving_entity)) {
    return blocked(TraversalBlockReason::occupied);
  }
  return TraversalAssessment{.reason = TraversalBlockReason::none, .cost = MovementCost{0}};
}

TraversalAssessment assess_transition(const CompiledHexMap& map, const OccupancyIndex& occupancy,
                                      const FootprintDefinition& footprint, const HexPose& from,
                                      const HexPose& destination, const TraversalPolicy policy,
                                      const EntityId moving_entity) {
  const auto placement = assess_placement(map, occupancy, footprint, destination, moving_entity);
  if (!placement.allowed()) {
    return placement;
  }

  if (from.anchor == destination.anchor) {
    const auto clockwise = rotate_clockwise(from.facing);
    const auto counterclockwise = rotate_counterclockwise(from.facing);
    if (destination.facing != clockwise && destination.facing != counterclockwise) {
      return blocked(TraversalBlockReason::invalid_transition);
    }
    return TraversalAssessment{.reason = TraversalBlockReason::none, .cost = policy.rotation_cost};
  }
  if (from.facing != destination.facing) {
    return blocked(TraversalBlockReason::invalid_transition);
  }

  const auto old_cells = footprint.expand(from);
  const auto new_cells = footprint.expand(destination);
  if (old_cells.size() != new_cells.size()) {
    return blocked(TraversalBlockReason::invalid_transition);
  }
  for (std::size_t index = 0; index < old_cells.size(); ++index) {
    const auto edge = map.edge(old_cells[index], new_cells[index]);
    if (!edge) {
      return blocked(TraversalBlockReason::missing_edge);
    }
    if (!edge->from_to(old_cells[index]).traversable) {
      return blocked(TraversalBlockReason::blocked_edge);
    }
  }

  const auto anchor_edge = map.edge(from.anchor, destination.anchor);
  const auto destination_cell = map.cell(destination.anchor);
  if (!anchor_edge) {
    return blocked(TraversalBlockReason::missing_edge);
  }
  if (!destination_cell) {
    return blocked(TraversalBlockReason::missing_cell);
  }
  const auto edge_cost = anchor_edge->from_to(from.anchor).cost;
  const auto total_cost = edge_cost.checked_add(destination_cell->base_cost.value());
  if (!total_cost) {
    return blocked(TraversalBlockReason::cost_overflow);
  }
  return TraversalAssessment{.reason = TraversalBlockReason::none, .cost = *total_cost};
}

} // namespace dross
