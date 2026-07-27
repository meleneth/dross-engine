#include <dross/hex/hex_coord.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>

TEST_CASE("axial and cube coordinates round trip exhaustively") {
  for (std::int32_t q = -8; q <= 8; ++q) {
    for (std::int32_t r = -8; r <= 8; ++r) {
      const dross::HexCoord axial{q, r};
      const auto cube = dross::to_cube(axial);
      CHECK(cube.x + cube.y + cube.z == 0);
      CHECK(dross::to_axial(cube) == axial);
    }
  }
}

TEST_CASE("hex distance is symmetric and satisfies the triangle inequality") {
  for (std::int32_t aq = -3; aq <= 3; ++aq) {
    for (std::int32_t ar = -3; ar <= 3; ++ar) {
      const dross::HexCoord a{aq, ar};
      const dross::HexCoord b{-ar, aq};
      const dross::HexCoord c{ar, -aq};
      CHECK(dross::hex_distance(a, b) == dross::hex_distance(b, a));
      CHECK(dross::hex_distance(a, c) <=
            dross::hex_distance(a, b) + dross::hex_distance(b, c));
    }
  }
}

TEST_CASE("direction values and vectors are stable clockwise") {
  constexpr std::array expected{
      dross::HexCoord{1, 0},  dross::HexCoord{0, 1},  dross::HexCoord{-1, 1},
      dross::HexCoord{-1, 0}, dross::HexCoord{0, -1}, dross::HexCoord{1, -1}};

  for (std::uint8_t value = 0; value < expected.size(); ++value) {
    const auto direction = static_cast<dross::HexDirection>(value);
    CHECK(dross::direction_offset(direction) == expected[value]);
    CHECK(dross::neighbor(dross::HexCoord{0, 0}, direction) == expected[value]);
    CHECK(dross::neighbor(expected[value], dross::opposite(direction)) ==
          dross::HexCoord{0, 0});
  }
}

TEST_CASE("six clockwise rotations return coordinates and facing to origin") {
  auto coordinate = dross::HexCoord{2, -1};
  auto facing = dross::HexFacing::east;
  for (int turn = 0; turn < 6; ++turn) {
    coordinate = dross::rotate_clockwise(coordinate);
    facing = dross::rotate_clockwise(facing);
  }
  CHECK(coordinate == dross::HexCoord{2, -1});
  CHECK(facing == dross::HexFacing::east);
  CHECK(dross::rotate_counterclockwise(dross::rotate_clockwise(coordinate)) ==
        coordinate);
}
