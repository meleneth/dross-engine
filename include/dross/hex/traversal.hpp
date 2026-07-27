#pragma once

#include <dross/foundation/quantities.hpp>
#include <dross/hex/compiled_hex_map.hpp>
#include <dross/hex/footprint.hpp>
#include <dross/hex/occupancy.hpp>

#include <cstdint>

namespace dross {

struct TraversalPolicy {
  MovementCost rotation_cost;
};

enum class TraversalBlockReason : std::uint8_t {
  none,
  invalid_transition,
  missing_cell,
  blocked_cell,
  occupied,
  missing_edge,
  blocked_edge,
  cost_overflow,
};

struct TraversalAssessment {
  TraversalBlockReason reason;
  MovementCost cost;

  [[nodiscard]] bool allowed() const noexcept { return reason == TraversalBlockReason::none; }
};

[[nodiscard]] TraversalAssessment assess_placement(const CompiledHexMap& map,
                                                   const OccupancyIndex& occupancy,
                                                   const FootprintDefinition& footprint,
                                                   const HexPose& pose, EntityId moving_entity);

[[nodiscard]] TraversalAssessment assess_transition(const CompiledHexMap& map,
                                                    const OccupancyIndex& occupancy,
                                                    const FootprintDefinition& footprint,
                                                    const HexPose& from, const HexPose& destination,
                                                    TraversalPolicy policy, EntityId moving_entity);

} // namespace dross
