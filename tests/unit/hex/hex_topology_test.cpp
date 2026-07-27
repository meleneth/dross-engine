#include <dross/hex/footprint.hpp>
#include <dross/hex/hex_topology.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>

namespace {

dross::RegionId region(const std::string_view text) {
  return dross::RegionId{dross::ContentId::parse(text).value()};
}

dross::HexCellId cell(const std::int32_t q, const std::int32_t r,
                      const std::int32_t layer = 0) {
  return dross::HexCellId{region("demo:room"), dross::HexCoord{q, r}, layer};
}

} // namespace

TEST_CASE("layers distinguish otherwise identical hex cells") {
  CHECK(cell(2, 3, 0) != cell(2, 3, 1));
  CHECK(cell(2, 3, 0) < cell(2, 3, 1));
}

TEST_CASE("edge keys canonicalize either endpoint order") {
  const auto forward = dross::EdgeKey::between(cell(0, 0), cell(1, 0));
  const auto backward = dross::EdgeKey::between(cell(1, 0), cell(0, 0));
  REQUIRE(forward);
  REQUIRE(backward);
  CHECK(*forward == *backward);
  CHECK(forward->first() == cell(0, 0));
  CHECK(forward->second() == cell(1, 0));
  CHECK_FALSE(dross::EdgeKey::between(cell(0, 0), cell(0, 0)));
}

TEST_CASE("footprints require an origin and reject duplicate offsets") {
  const auto footprint_id =
      dross::FootprintId{dross::ContentId::parse("demo:shape").value()};
  const auto missing_origin =
      dross::FootprintDefinition::create(footprint_id, {dross::HexCoord{1, 0}});
  REQUIRE_FALSE(missing_origin);
  CHECK(missing_origin.error() == dross::FootprintError::missing_origin);

  const auto duplicate = dross::FootprintDefinition::create(
      footprint_id, {dross::HexCoord{0, 0}, dross::HexCoord{0, 0}});
  REQUIRE_FALSE(duplicate);
  CHECK(duplicate.error() == dross::FootprintError::duplicate_offset);
}

TEST_CASE("asymmetric footprint rotations are canonical and cycle after six turns") {
  const auto footprint = dross::FootprintDefinition::create(
                             dross::FootprintId{
                                 dross::ContentId::parse("demo:asymmetric").value()},
                             {dross::HexCoord{1, 0}, dross::HexCoord{0, 0},
                              dross::HexCoord{1, -1}})
                             .value();
  CHECK(std::ranges::is_sorted(footprint.offsets()));

  const auto east = footprint.rotated(dross::HexFacing::east);
  const auto southeast = footprint.rotated(dross::HexFacing::southeast);
  CHECK(east != southeast);
  CHECK(footprint.rotated(static_cast<dross::HexFacing>(6)) == east);
}

TEST_CASE("footprint expansion preserves region and layer") {
  const auto footprint =
      dross::FootprintDefinition::create(
          dross::FootprintId{dross::ContentId::parse("demo:two_cell").value()},
          {dross::HexCoord{0, 0}, dross::HexCoord{1, 0}})
          .value();
  const dross::HexPose pose{cell(4, -2, 7), dross::HexFacing::west};
  const auto expanded = footprint.expand(pose);

  const std::array expected{cell(3, -2, 7), cell(4, -2, 7)};
  CHECK(std::ranges::equal(expanded, expected));
}
