#include <dross/hex/path_planner.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <string_view>
#include <vector>

namespace {

dross::RegionId path_region() {
  return dross::RegionId{dross::ContentId::parse("demo:path").value()};
}

dross::HexCellId path_cell(const std::int32_t q, const std::int32_t r) {
  return dross::HexCellId{path_region(), dross::HexCoord{q, r}, 0};
}

dross::CellFacts path_facts(const dross::HexCellId& id, const bool traversable = true,
                            const std::uint32_t cost = 0) {
  return dross::CellFacts{.id = id,
                          .surface_height = dross::Millimeters{0},
                          .terrain = dross::ContentId::parse("dross:floor").value(),
                          .base_cost = dross::MovementCost{cost},
                          .clearance = dross::Clearance::open,
                          .traversable = traversable,
                          .semantic_tags = {}};
}

dross::DirectionalEdgeFacts open_edge(const std::uint32_t cost = 1) {
  return dross::DirectionalEdgeFacts{.traversable = true, .cost = dross::MovementCost{cost}};
}

dross::DirectionalEdgeFacts closed_edge() {
  return dross::DirectionalEdgeFacts{.traversable = false, .cost = dross::MovementCost{1}};
}

dross::FootprintDefinition single_cell_footprint() {
  return dross::FootprintDefinition::create(
             dross::FootprintId{dross::ContentId::parse("demo:single").value()},
             {dross::HexCoord{0, 0}})
      .value();
}

dross::CompiledHexMap build_diamond(const bool reverse_insertion,
                                    const std::uint32_t upper_cost = 1,
                                    const std::uint32_t lower_cost = 1) {
  const std::vector cells{path_cell(0, 0), path_cell(0, 1), path_cell(1, 0), path_cell(1, 1)};
  dross::CompiledHexMapBuilder builder;
  if (reverse_insertion) {
    for (auto iterator = cells.rbegin(); iterator != cells.rend(); ++iterator) {
      REQUIRE(builder.add_cell(path_facts(*iterator)));
    }
  } else {
    for (const auto& cell : cells) {
      REQUIRE(builder.add_cell(path_facts(cell)));
    }
  }
  REQUIRE(builder.add_edge(cells[0], cells[1], open_edge(lower_cost), open_edge(lower_cost)));
  REQUIRE(builder.add_edge(cells[1], cells[3], open_edge(lower_cost), open_edge(lower_cost)));
  REQUIRE(builder.add_edge(cells[0], cells[2], open_edge(upper_cost), open_edge(upper_cost)));
  REQUIRE(builder.add_edge(cells[2], cells[3], open_edge(upper_cost), open_edge(upper_cost)));
  return std::move(builder).build().value();
}

} // namespace

TEST_CASE("traversal assessment reports blocked cells, edges, and occupancy") {
  dross::CompiledHexMapBuilder builder;
  REQUIRE(builder.add_cell(path_facts(path_cell(0, 0))));
  REQUIRE(builder.add_cell(path_facts(path_cell(1, 0))));
  REQUIRE(builder.add_cell(path_facts(path_cell(0, 1), false)));
  REQUIRE(builder.add_edge(path_cell(0, 0), path_cell(1, 0), closed_edge(), open_edge()));
  REQUIRE(builder.add_edge(path_cell(0, 0), path_cell(0, 1), open_edge(), open_edge()));
  const auto map = std::move(builder).build().value();
  const auto footprint = single_cell_footprint();
  dross::OccupancyIndex occupancy;
  const dross::EntityId actor{1, 1};
  const dross::EntityId blocker{1, 2};

  auto assessment = dross::assess_transition(
      map, occupancy, footprint, dross::HexPose{path_cell(0, 0), dross::HexFacing::east},
      dross::HexPose{path_cell(1, 0), dross::HexFacing::east},
      dross::TraversalPolicy{dross::MovementCost{1}}, actor);
  CHECK(assessment.reason == dross::TraversalBlockReason::blocked_edge);

  assessment = dross::assess_transition(map, occupancy, footprint,
                                        dross::HexPose{path_cell(0, 0), dross::HexFacing::east},
                                        dross::HexPose{path_cell(0, 1), dross::HexFacing::east},
                                        dross::TraversalPolicy{dross::MovementCost{1}}, actor);
  CHECK(assessment.reason == dross::TraversalBlockReason::blocked_cell);

  REQUIRE(occupancy.place(blocker, {path_cell(1, 0)}));
  assessment = dross::assess_placement(
      map, occupancy, footprint, dross::HexPose{path_cell(1, 0), dross::HexFacing::east}, actor);
  CHECK(assessment.reason == dross::TraversalBlockReason::occupied);
}

TEST_CASE("weighted planner selects the least-cost route") {
  const auto map = build_diamond(false, 5, 1);
  const auto footprint = single_cell_footprint();
  const dross::OccupancyIndex occupancy;
  const dross::WeightedAStarPathPlanner planner;
  const auto result = planner.plan(
      map, occupancy, footprint, dross::HexPose{path_cell(0, 0), dross::HexFacing::east},
      dross::HexPose{path_cell(1, 1), dross::HexFacing::east},
      dross::TraversalPolicy{dross::MovementCost{1}}, dross::EntityId{1, 1});

  REQUIRE(result);
  REQUIRE(result->poses.size() == 3);
  CHECK(result->poses[1].anchor == path_cell(0, 1));
  CHECK(result->total_cost == dross::MovementCost{2});
}

TEST_CASE("planner tie-breaking is independent of map insertion order") {
  const auto first_map = build_diamond(false);
  const auto second_map = build_diamond(true);
  const auto footprint = single_cell_footprint();
  const dross::OccupancyIndex occupancy;
  const dross::WeightedAStarPathPlanner planner;
  const dross::HexPose start{path_cell(0, 0), dross::HexFacing::east};
  const dross::HexPose goal{path_cell(1, 1), dross::HexFacing::east};
  const dross::TraversalPolicy policy{dross::MovementCost{1}};

  const auto first =
      planner.plan(first_map, occupancy, footprint, start, goal, policy, dross::EntityId{1, 1});
  const auto second =
      planner.plan(second_map, occupancy, footprint, start, goal, policy, dross::EntityId{1, 1});
  REQUIRE(first);
  REQUIRE(second);
  CHECK(first->poses == second->poses);
}

TEST_CASE("planner optimality is exhaustive across the bounded diamond blockers") {
  const auto footprint = single_cell_footprint();
  const dross::OccupancyIndex occupancy;
  const dross::WeightedAStarPathPlanner planner;
  for (std::uint8_t blocker_mask = 0; blocker_mask < 4; ++blocker_mask) {
    dross::CompiledHexMapBuilder builder;
    REQUIRE(builder.add_cell(path_facts(path_cell(0, 0))));
    REQUIRE(builder.add_cell(path_facts(path_cell(0, 1), (blocker_mask & 1U) == 0)));
    REQUIRE(builder.add_cell(path_facts(path_cell(1, 0), (blocker_mask & 2U) == 0)));
    REQUIRE(builder.add_cell(path_facts(path_cell(1, 1))));
    REQUIRE(builder.add_edge(path_cell(0, 0), path_cell(0, 1), open_edge(), open_edge()));
    REQUIRE(builder.add_edge(path_cell(0, 1), path_cell(1, 1), open_edge(), open_edge()));
    REQUIRE(builder.add_edge(path_cell(0, 0), path_cell(1, 0), open_edge(), open_edge()));
    REQUIRE(builder.add_edge(path_cell(1, 0), path_cell(1, 1), open_edge(), open_edge()));
    const auto map = std::move(builder).build().value();
    const auto result = planner.plan(
        map, occupancy, footprint, dross::HexPose{path_cell(0, 0), dross::HexFacing::east},
        dross::HexPose{path_cell(1, 1), dross::HexFacing::east},
        dross::TraversalPolicy{dross::MovementCost{1}}, dross::EntityId{1, 1});

    if (blocker_mask == 3) {
      REQUIRE_FALSE(result);
      CHECK(result.error() == dross::PathError::no_path);
    } else {
      REQUIRE(result);
      CHECK(result->total_cost == dross::MovementCost{2});
      CHECK(result->poses.size() == 3);
    }
  }
}

TEST_CASE("asymmetric footprint rotates to traverse a narrow route") {
  dross::CompiledHexMapBuilder builder;
  for (const auto& cell : {path_cell(0, 0), path_cell(1, 0), path_cell(0, 1), path_cell(0, 2)}) {
    REQUIRE(builder.add_cell(path_facts(cell)));
  }
  REQUIRE(builder.add_edge(path_cell(0, 0), path_cell(0, 1), open_edge(), open_edge()));
  REQUIRE(builder.add_edge(path_cell(0, 1), path_cell(0, 2), open_edge(), open_edge()));
  const auto map = std::move(builder).build().value();
  const auto footprint =
      dross::FootprintDefinition::create(
          dross::FootprintId{dross::ContentId::parse("demo:asymmetric_two").value()},
          {dross::HexCoord{0, 0}, dross::HexCoord{1, 0}})
          .value();
  const dross::WeightedAStarPathPlanner planner;
  const dross::OccupancyIndex occupancy;

  const auto result = planner.plan(
      map, occupancy, footprint, dross::HexPose{path_cell(0, 0), dross::HexFacing::east},
      dross::HexPose{path_cell(0, 1), dross::HexFacing::southeast},
      dross::TraversalPolicy{dross::MovementCost{1}}, dross::EntityId{1, 1});
  REQUIRE(result);
  CHECK(result->poses.size() == 3);
  CHECK(result->poses[1].anchor == path_cell(0, 0));
  CHECK(result->poses[1].facing == dross::HexFacing::southeast);
}

TEST_CASE("closed edge yields no path and occupancy changes stale a plan") {
  dross::CompiledHexMapBuilder builder;
  REQUIRE(builder.add_cell(path_facts(path_cell(0, 0))));
  REQUIRE(builder.add_cell(path_facts(path_cell(1, 0))));
  REQUIRE(builder.add_edge(path_cell(0, 0), path_cell(1, 0), closed_edge(), closed_edge()));
  const auto map = std::move(builder).build().value();
  const auto footprint = single_cell_footprint();
  dross::OccupancyIndex occupancy;
  const dross::WeightedAStarPathPlanner planner;
  const auto result = planner.plan(
      map, occupancy, footprint, dross::HexPose{path_cell(0, 0), dross::HexFacing::east},
      dross::HexPose{path_cell(1, 0), dross::HexFacing::east},
      dross::TraversalPolicy{dross::MovementCost{1}}, dross::EntityId{1, 1});
  REQUIRE_FALSE(result);
  CHECK(result.error() == dross::PathError::no_path);

  const auto open_map = build_diamond(false);
  const auto valid_plan = planner.plan(
      open_map, occupancy, footprint, dross::HexPose{path_cell(0, 0), dross::HexFacing::east},
      dross::HexPose{path_cell(1, 1), dross::HexFacing::east},
      dross::TraversalPolicy{dross::MovementCost{1}}, dross::EntityId{1, 1});
  REQUIRE(valid_plan);
  CHECK(valid_plan->matches(occupancy));
  REQUIRE(occupancy.place(dross::EntityId{1, 2}, {path_cell(3, 3)}));
  CHECK_FALSE(valid_plan->matches(occupancy));
}
