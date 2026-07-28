#include <dross/hex/grid_bake.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

namespace {

dross::ContentId id(const char* value) { return dross::ContentId::parse(value).value(); }

dross::GridIdentity identity(const std::int64_t radius = 1000) {
  return dross::GridIdentity{
      .region = dross::RegionId{id("demo:room")},
      .origin_x_mm = 0,
      .origin_y_mm = 0,
      .origin_z_mm = 0,
      .radius_mm = radius,
  };
}

dross::HexCellId cell(const std::int32_t q, const std::int32_t r) {
  return dross::HexCellId{.region = identity().region, .coord = {.q = q, .r = r}, .layer = 0};
}

dross::CellBakeEvidence evidence(const std::int32_t q, const std::int32_t r,
                                 std::vector<std::int64_t> samples, const bool clearance = true) {
  return dross::CellBakeEvidence{
      .id = cell(q, r),
      .surface_samples_mm = std::move(samples),
      .standing_clearance = clearance,
      .source = id("demo:floor_mesh"),
  };
}

dross::HexBakeProfile profile() {
  return dross::HexBakeProfile{
      .algorithm_version = 1,
      .quantization_mm = 10,
      .maximum_height_variance_mm = 30,
      .required_sample_count = 4,
      .terrain = id("dross:floor"),
      .movement_cost = dross::MovementCost{1},
  };
}

} // namespace

TEST_CASE("multi-sample evidence quantizes a coherent walkable surface") {
  const auto result = dross::classify_bake_cell(evidence(0, 0, {1001, 1004, 996, 999}), profile());
  REQUIRE(result);
  CHECK(result->surface_height == dross::Millimeters{1000});
  CHECK(result->traversable);
  CHECK(result->reason == dross::BakeReason::automatic_traversable);
}

TEST_CASE("bake classification retains blocked reason diagnostics") {
  const auto uneven = dross::classify_bake_cell(evidence(0, 0, {900, 920, 940, 960}), profile());
  REQUIRE(uneven);
  CHECK_FALSE(uneven->traversable);
  CHECK(uneven->reason == dross::BakeReason::height_variance);

  const auto blocked =
      dross::classify_bake_cell(evidence(0, 0, {1000, 1000, 1000, 1000}, false), profile());
  REQUIRE(blocked);
  CHECK(blocked->reason == dross::BakeReason::insufficient_clearance);
  CHECK(blocked->clearance == dross::Clearance::blocked);

  CHECK_FALSE(dross::classify_bake_cell(evidence(0, 0, {1000}), profile()));
}

TEST_CASE("manual traversability overrides survive rebake and retain provenance") {
  dross::GridBake bake{
      .identity = identity(),
      .profile_version = 1,
      .cells = {evidence(0, 0, {1000, 1000, 1000, 1000}, false),
                evidence(1, 0, {1000, 1000, 1000, 1000})},
      .edges = {},
  };
  dross::GridOverrides overrides{
      .identity = identity(),
      .cells = {{cell(0, 0), dross::CellTraversabilityOverride::force_traversable}},
  };

  const auto first = dross::compile_grid_bake(bake, overrides, profile());
  REQUIRE(first);
  REQUIRE(first->map.cell(cell(0, 0)));
  CHECK(first->map.cell(cell(0, 0))->traversable);
  CHECK(first->provenance.at(cell(0, 0)) == dross::CellProvenance::manual_force_traversable);

  bake.cells[0].standing_clearance = true;
  const auto rebaked = dross::compile_grid_bake(bake, overrides, profile());
  REQUIRE(rebaked);
  CHECK(rebaked->provenance.at(cell(0, 0)) == dross::CellProvenance::manual_force_traversable);
}

TEST_CASE("grid identity changes and missing cells surface orphan overrides") {
  const dross::GridBake bake{
      .identity = identity(),
      .profile_version = 1,
      .cells = {evidence(0, 0, {1000, 1000, 1000, 1000})},
      .edges = {},
  };
  auto overrides = dross::GridOverrides{
      .identity = identity(900),
      .cells = {{cell(0, 0), dross::CellTraversabilityOverride::force_blocked}},
  };
  auto result = dross::compile_grid_bake(bake, overrides, profile());
  REQUIRE_FALSE(result);
  CHECK(result.error().reason == dross::GridCompileErrorReason::identity_mismatch);

  overrides.identity = identity();
  overrides.cells = {{cell(2, 0), dross::CellTraversabilityOverride::force_blocked}};
  result = dross::compile_grid_bake(bake, overrides, profile());
  REQUIRE_FALSE(result);
  CHECK(result.error().reason == dross::GridCompileErrorReason::orphan_override);
  REQUIRE(result.error().cell);
  CHECK(*result.error().cell == cell(2, 0));
}

TEST_CASE("compiled bake is canonical regardless of evidence and override input order") {
  auto bake = dross::GridBake{
      .identity = identity(),
      .profile_version = 1,
      .cells = {evidence(1, 0, {1000, 1000, 1000, 1000}), evidence(0, 0, {1000, 1000, 1000, 1000})},
      .edges = {dross::EdgeBakeEvidence{
          .from = cell(1, 0),
          .to = cell(0, 0),
          .from_to_clear = true,
          .to_from_clear = false,
      }},
  };
  dross::GridOverrides overrides{
      .identity = identity(),
      .cells = {{cell(1, 0), dross::CellTraversabilityOverride::force_blocked},
                {cell(0, 0), dross::CellTraversabilityOverride::automatic}},
  };
  const auto first = dross::compile_grid_bake(bake, overrides, profile());
  REQUIRE(first);

  std::ranges::reverse(bake.cells);
  std::ranges::reverse(overrides.cells);
  const auto second = dross::compile_grid_bake(bake, overrides, profile());
  REQUIRE(second);
  CHECK(first->map.cell_ids() == second->map.cell_ids());
  CHECK(first->provenance == second->provenance);
  REQUIRE(first->map.edge(cell(0, 0), cell(1, 0)));
  REQUIRE(second->map.edge(cell(0, 0), cell(1, 0)));
  CHECK(first->map.edge(cell(0, 0), cell(1, 0))->from_to(cell(0, 0)) ==
        second->map.edge(cell(0, 0), cell(1, 0))->from_to(cell(0, 0)));
}
