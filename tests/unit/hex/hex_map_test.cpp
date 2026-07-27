#include <dross/hex/compiled_hex_map.hpp>
#include <dross/hex/occupancy.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <string_view>

namespace {

dross::RegionId map_region() {
  return dross::RegionId{dross::ContentId::parse("demo:map").value()};
}

dross::HexCellId map_cell(const std::int32_t q, const std::int32_t r,
                          const std::int32_t layer = 0) {
  return dross::HexCellId{map_region(), dross::HexCoord{q, r}, layer};
}

dross::CellFacts facts(const dross::HexCellId& id, const bool traversable = true,
                       const std::uint32_t cost = 1) {
  return dross::CellFacts{.id = id,
                          .surface_height = dross::Millimeters{0},
                          .terrain = dross::ContentId::parse("dross:floor").value(),
                          .base_cost = dross::MovementCost{cost},
                          .clearance = dross::Clearance::open,
                          .traversable = traversable,
                          .semantic_tags = {}};
}

} // namespace

TEST_CASE("compiled maps expose cells and edges in canonical order") {
  dross::CompiledHexMapBuilder builder;
  REQUIRE(builder.add_cell(facts(map_cell(1, 0))));
  REQUIRE(builder.add_cell(facts(map_cell(0, 0))));
  REQUIRE(builder.add_cell(facts(map_cell(0, 1))));
  REQUIRE(builder.add_edge(
      map_cell(1, 0), map_cell(0, 0),
      dross::DirectionalEdgeFacts{.traversable = false, .cost = dross::MovementCost{9}},
      dross::DirectionalEdgeFacts{.traversable = true, .cost = dross::MovementCost{2}}));
  const auto map = std::move(builder).build();
  REQUIRE(map);

  CHECK(std::ranges::is_sorted(map->cell_ids()));
  const auto edge = map->edge(map_cell(0, 0), map_cell(1, 0));
  REQUIRE(edge);
  CHECK(edge->from_to(map_cell(1, 0)).traversable == false);
  CHECK(edge->from_to(map_cell(0, 0)).cost == dross::MovementCost{2});
}

TEST_CASE("missing explicit edges do not create neighbors") {
  dross::CompiledHexMapBuilder builder;
  REQUIRE(builder.add_cell(facts(map_cell(0, 0))));
  REQUIRE(builder.add_cell(facts(map_cell(1, 0))));
  const auto map = std::move(builder).build().value();

  CHECK(map.neighbors(map_cell(0, 0)).empty());
}

TEST_CASE("map builder rejects duplicate cells and edges with missing endpoints") {
  dross::CompiledHexMapBuilder builder;
  REQUIRE(builder.add_cell(facts(map_cell(0, 0))));
  const auto duplicate = builder.add_cell(facts(map_cell(0, 0)));
  REQUIRE_FALSE(duplicate);
  CHECK(duplicate.error() == dross::MapBuildError::duplicate_cell);

  const auto missing = builder.add_edge(
      map_cell(0, 0), map_cell(1, 0),
      dross::DirectionalEdgeFacts{.traversable = true, .cost = dross::MovementCost{1}},
      dross::DirectionalEdgeFacts{.traversable = true, .cost = dross::MovementCost{1}});
  REQUIRE_FALSE(missing);
  CHECK(missing.error() == dross::MapBuildError::missing_cell);
}

TEST_CASE("occupancy detects every cell in a multi-cell placement") {
  dross::OccupancyIndex occupancy;
  const dross::EntityId first{1, 1};
  const dross::EntityId second{1, 2};
  REQUIRE(occupancy.place(first, {map_cell(0, 0), map_cell(1, 0)}));

  const auto conflict = occupancy.place(second, {map_cell(1, 0), map_cell(2, 0)});
  REQUIRE_FALSE(conflict);
  CHECK(conflict.error().reason == dross::OccupancyErrorReason::occupied);
  CHECK(conflict.error().cell == map_cell(1, 0));
  CHECK(occupancy.occupant(map_cell(1, 0)) == first);
  CHECK_FALSE(occupancy.occupant(map_cell(2, 0)));
}

TEST_CASE("occupancy move and removal update revision explicitly") {
  dross::OccupancyIndex occupancy;
  const dross::EntityId actor{2, 1};
  REQUIRE(occupancy.place(actor, {map_cell(0, 0)}));
  const auto placed_revision = occupancy.revision();
  REQUIRE(occupancy.move(actor, {map_cell(1, 0), map_cell(1, -1)}));
  CHECK(occupancy.revision() == placed_revision + 1);
  CHECK_FALSE(occupancy.occupant(map_cell(0, 0)));
  CHECK(occupancy.occupant(map_cell(1, -1)) == actor);
  REQUIRE(occupancy.remove(actor));
  CHECK(occupancy.empty());
}

TEST_CASE("occupancy deterministic rebuild equals incremental placement") {
  const std::vector placements{
      dross::OccupancyPlacement{.entity = dross::EntityId{3, 2}, .cells = {map_cell(2, 0)}},
      dross::OccupancyPlacement{.entity = dross::EntityId{3, 1},
                                .cells = {map_cell(0, 0), map_cell(1, 0)}}};
  dross::OccupancyIndex rebuilt;
  REQUIRE(rebuilt.rebuild(placements));

  dross::OccupancyIndex incremental;
  REQUIRE(incremental.place(placements[1].entity, placements[1].cells));
  REQUIRE(incremental.place(placements[0].entity, placements[0].cells));
  CHECK(rebuilt.entries() == incremental.entries());
}
