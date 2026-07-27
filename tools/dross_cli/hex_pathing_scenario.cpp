#include "hex_pathing_scenario.hpp"

#include <dross/hex/path_planner.hpp>

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string_view>
#include <utility>

namespace {

constexpr int scenario_error = 5;

[[nodiscard]] dross::RegionId fixture_region() {
  return dross::RegionId{dross::ContentId::parse("demo:hex_pathing").value()};
}

[[nodiscard]] dross::HexCellId fixture_cell(const std::int32_t q, const std::int32_t r) {
  return dross::HexCellId{
      .region = fixture_region(), .coord = dross::HexCoord{.q = q, .r = r}, .layer = 0};
}

[[nodiscard]] dross::CellFacts fixture_facts(const dross::HexCellId& cell_id) {
  return dross::CellFacts{.id = cell_id,
                          .surface_height = dross::Millimeters{0},
                          .terrain = dross::ContentId::parse("dross:floor").value(),
                          .base_cost = dross::MovementCost{0},
                          .clearance = dross::Clearance::open,
                          .traversable = true,
                          .semantic_tags = {}};
}

[[nodiscard]] dross::DirectionalEdgeFacts open_edge() {
  return dross::DirectionalEdgeFacts{.traversable = true, .cost = dross::MovementCost{1}};
}

[[nodiscard]] dross::DirectionalEdgeFacts closed_edge() {
  return dross::DirectionalEdgeFacts{.traversable = false, .cost = dross::MovementCost{1}};
}

[[nodiscard]] dross::Result<dross::CompiledHexMap, dross::MapBuildError> build_fixture() {
  const auto anchor = fixture_cell(0, 0);
  const auto east = fixture_cell(1, 0);
  const auto southeast = fixture_cell(0, 1);
  const auto far_southeast = fixture_cell(0, 2);
  dross::CompiledHexMapBuilder builder;
  for (const auto& cell_id : {far_southeast, east, anchor, southeast}) {
    const auto added = builder.add_cell(fixture_facts(cell_id));
    if (!added) {
      return tl::unexpected{added.error()};
    }
  }
  auto edge_added = builder.add_edge(anchor, southeast, open_edge(), open_edge());
  if (!edge_added) {
    return tl::unexpected{edge_added.error()};
  }
  edge_added = builder.add_edge(southeast, far_southeast, open_edge(), open_edge());
  if (!edge_added) {
    return tl::unexpected{edge_added.error()};
  }
  edge_added = builder.add_edge(anchor, east, closed_edge(), closed_edge());
  if (!edge_added) {
    return tl::unexpected{edge_added.error()};
  }
  return std::move(builder).build();
}

[[nodiscard]] std::string_view facing_name(const dross::HexFacing facing) {
  switch (facing) {
  case dross::HexFacing::east:
    return "east";
  case dross::HexFacing::southeast:
    return "southeast";
  case dross::HexFacing::southwest:
    return "southwest";
  case dross::HexFacing::west:
    return "west";
  case dross::HexFacing::northwest:
    return "northwest";
  case dross::HexFacing::northeast:
    return "northeast";
  }
  return "invalid";
}

void print_path(const dross::PlannedPath& path) {
  std::cout << '[';
  for (std::size_t index = 0; index < path.poses.size(); ++index) {
    if (index != 0) {
      std::cout << '>';
    }
    const auto& pose = path.poses[index];
    std::cout << pose.anchor.coord.q << ',' << pose.anchor.coord.r << ','
              << facing_name(pose.facing);
  }
  std::cout << ']';
}

[[nodiscard]] std::string_view block_name(const dross::TraversalBlockReason reason) {
  switch (reason) {
  case dross::TraversalBlockReason::occupied:
    return "occupied";
  case dross::TraversalBlockReason::blocked_edge:
    return "blocked_edge";
  default:
    return "unexpected";
  }
}

} // namespace

int run_hex_pathing_scenario() {
  auto map = build_fixture();
  if (!map) {
    std::cerr << "hex pathing fixture failed\n";
    return scenario_error;
  }
  const auto single = dross::FootprintDefinition::create(
                          dross::FootprintId{dross::ContentId::parse("demo:single").value()},
                          {dross::HexCoord{.q = 0, .r = 0}})
                          .value();
  const auto asymmetric =
      dross::FootprintDefinition::create(
          dross::FootprintId{dross::ContentId::parse("demo:asymmetric").value()},
          {dross::HexCoord{.q = 0, .r = 0}, dross::HexCoord{.q = 1, .r = 0}})
          .value();
  const dross::WeightedAStarPathPlanner implementation;
  const dross::PathPlanner& planner = implementation;
  dross::OccupancyIndex occupancy;
  const dross::EntityId actor{9, 1};
  const dross::TraversalPolicy policy{dross::MovementCost{1}};
  const dross::HexPose start{.anchor = fixture_cell(0, 0), .facing = dross::HexFacing::east};
  const auto single_path =
      planner.plan(*map, occupancy, single, start,
                   dross::HexPose{.anchor = fixture_cell(0, 1), .facing = dross::HexFacing::east},
                   policy, actor);
  const auto asymmetric_path = planner.plan(
      *map, occupancy, asymmetric, start,
      dross::HexPose{.anchor = fixture_cell(0, 1), .facing = dross::HexFacing::southeast}, policy,
      actor);
  if (!single_path || !asymmetric_path) {
    std::cerr << "hex pathing plan failed\n";
    return scenario_error;
  }

  std::size_t rotations = 0;
  for (std::size_t index = 1; index < asymmetric_path->poses.size(); ++index) {
    if (asymmetric_path->poses[index - 1].anchor == asymmetric_path->poses[index].anchor) {
      ++rotations;
    }
  }
  const dross::EntityId blocker{9, 2};
  if (!occupancy.place(blocker, {fixture_cell(0, 2)})) {
    return scenario_error;
  }
  const auto occupied = dross::assess_placement(
      *map, occupancy, asymmetric,
      dross::HexPose{.anchor = fixture_cell(0, 1), .facing = dross::HexFacing::southeast}, actor);
  const auto edge_blocked = dross::assess_transition(
      *map, occupancy, single, start,
      dross::HexPose{.anchor = fixture_cell(1, 0), .facing = dross::HexFacing::east}, policy,
      actor);

  std::cout << "hex-pathing single=";
  print_path(*single_path);
  std::cout << " cost=" << single_path->total_cost.value() << " asymmetric=";
  print_path(*asymmetric_path);
  std::cout << " cost=" << asymmetric_path->total_cost.value() << " rotations=" << rotations
            << " blocked=[" << block_name(occupied.reason) << ',' << block_name(edge_blocked.reason)
            << "] revision=" << occupancy.revision() << '\n';
  return 0;
}
