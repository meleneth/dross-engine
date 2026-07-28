#include <dross/runtime/door_runtime.hpp>

#include <dross/hex/path_planner.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

namespace {

dross::ContentId id(const char* value) { return dross::ContentId::parse(value).value(); }

dross::HexCellId cell(const std::int32_t q, const std::int32_t r = 0) {
  return {.region = dross::RegionId{id("demo:room")}, .coord = {.q = q, .r = r}, .layer = 0};
}

dross::EdgeKey edge(const std::int32_t first, const std::int32_t second) {
  return dross::EdgeKey::between(cell(first), cell(second)).value();
}

dross::CellFacts cell_facts(const dross::HexCellId& cell_id) {
  return {.id = cell_id,
          .surface_height = dross::Millimeters{0},
          .terrain = id("dross:floor"),
          .base_cost = dross::MovementCost{0},
          .clearance = dross::Clearance::open,
          .traversable = true,
          .semantic_tags = {}};
}

dross::DirectionalEdgeFacts traversable_edge() {
  return {.traversable = true, .cost = dross::MovementCost{1}};
}

dross::FootprintDefinition single_cell_footprint() {
  return dross::FootprintDefinition::create(dross::FootprintId{id("demo:single")},
                                            {dross::HexCoord{0, 0}})
      .value();
}

constexpr dross::EntityRef door_entity() {
  return {dross::WorldInstanceId{1}, dross::EntityId{7, 9}};
}

class RecordingDoorEvents final : public dross::DoorRuntime::EventSink {
public:
  void publish(const dross::door::DoorOpened&) override { calls.emplace_back("opened"); }
  void publish(const dross::door::DoorClosed&) override { calls.emplace_back("closed"); }
  std::vector<std::string> calls;
};

} // namespace

TEST_CASE("edge footprint requires unique adjacent edges") {
  const auto valid = dross::EdgeFootprint::create({edge(1, 0), edge(2, 1)});
  REQUIRE(valid);
  CHECK(valid->edges() == std::vector{edge(0, 1), edge(1, 2)});

  CHECK_FALSE(dross::EdgeFootprint::create({}));
  CHECK_FALSE(dross::EdgeFootprint::create({edge(0, 1), edge(1, 0)}));
  const auto non_adjacent = dross::EdgeKey::between(cell(0), cell(2)).value();
  CHECK_FALSE(dross::EdgeFootprint::create({non_adjacent}));
}

TEST_CASE("door lifecycle commits open and closed traversal state through SML") {
  auto footprint = dross::EdgeFootprint::create({edge(0, 1)}).value();
  RecordingDoorEvents events;
  dross::DoorRuntime door{door_entity(), std::move(footprint), dross::DoorState::closed, &events};

  CHECK_FALSE(door.allows(edge(0, 1)));
  REQUIRE(door.open());
  CHECK(door.state() == dross::DoorState::open);
  CHECK(events.calls == std::vector<std::string>{"opened"});
  CHECK(door.allows(edge(1, 0)));
  CHECK_FALSE(door.open());
  REQUIRE(door.close());
  CHECK(door.state() == dross::DoorState::closed);
  CHECK(events.calls == std::vector<std::string>{"opened", "closed"});
  CHECK_FALSE(door.allows(edge(0, 1)));
  CHECK_FALSE(door.close());
}

TEST_CASE("authoritative planner consumes committed door traversal state") {
  dross::CompiledHexMapBuilder builder;
  REQUIRE(builder.add_cell(cell_facts(cell(0))));
  REQUIRE(builder.add_cell(cell_facts(cell(1))));
  REQUIRE(builder.add_edge(cell(0), cell(1), traversable_edge(), traversable_edge()));
  const auto map = std::move(builder).build().value();
  auto door_footprint = dross::EdgeFootprint::create({edge(0, 1)}).value();
  dross::DoorRuntime door{door_entity(), std::move(door_footprint), dross::DoorState::closed};
  const dross::WeightedAStarPathPlanner planner;
  const dross::OccupancyIndex occupancy;
  const auto actor = dross::EntityId{7, 1};
  const auto start = dross::HexPose{cell(0), dross::HexFacing::east};
  const auto goal = dross::HexPose{cell(1), dross::HexFacing::east};
  const auto footprint = single_cell_footprint();
  const auto policy =
      dross::TraversalPolicy{.rotation_cost = dross::MovementCost{1}, .edge_policy = &door};

  const auto blocked = planner.plan(map, occupancy, footprint, start, goal, policy, actor);
  REQUIRE_FALSE(blocked);
  CHECK(blocked.error() == dross::PathError::no_path);

  REQUIRE(door.open());
  const auto opened = planner.plan(map, occupancy, footprint, start, goal, policy, actor);
  REQUIRE(opened);
  CHECK(opened->poses == std::vector{start, goal});
  CHECK(opened->total_cost == dross::MovementCost{1});
}

TEST_CASE("presentation acknowledgement cannot change committed door state") {
  auto footprint = dross::EdgeFootprint::create({edge(0, 1)}).value();
  dross::DoorRuntime door{door_entity(), std::move(footprint), dross::DoorState::closed, nullptr,
                          2};
  REQUIRE(door.open());
  const auto committed = door.state();
  const auto acknowledgement = door.presentation_acknowledgement_id();
  REQUIRE(acknowledgement != 0);
  CHECK(door.presentation_pending());

  CHECK_FALSE(door.acknowledge_presentation(acknowledgement + 1));
  CHECK(door.presentation_pending());
  REQUIRE(door.acknowledge_presentation(acknowledgement));
  CHECK_FALSE(door.presentation_pending());
  CHECK_FALSE(door.acknowledge_presentation(acknowledgement));
  CHECK(door.state() == committed);
  CHECK(door.allows(edge(0, 1)));
}

TEST_CASE("missing presentation acknowledgement times out without changing door truth") {
  auto footprint = dross::EdgeFootprint::create({edge(0, 1)}).value();
  dross::DoorRuntime door{door_entity(), std::move(footprint), dross::DoorState::closed, nullptr,
                          2};
  REQUIRE(door.open());
  const auto acknowledgement = door.presentation_acknowledgement_id();

  CHECK_FALSE(door.advance_presentation());
  CHECK(door.presentation_pending());
  REQUIRE(door.advance_presentation());
  CHECK_FALSE(door.presentation_pending());
  CHECK_FALSE(door.acknowledge_presentation(acknowledgement));
  CHECK(door.state() == dross::DoorState::open);
  CHECK(door.allows(edge(0, 1)));
}

TEST_CASE("door snapshot codec restores authoritative state without presentation data") {
  auto footprint = dross::EdgeFootprint::create({edge(0, 1)}).value();
  dross::DoorRuntime original{door_entity(), std::move(footprint), dross::DoorState::closed};
  REQUIRE(original.open());
  static_cast<void>(original.acknowledge_presentation(99));

  dross::ByteWriter writer;
  dross::encode_door_snapshot(writer, original.snapshot());
  dross::ByteReader reader{writer.bytes()};
  const auto decoded = dross::decode_door_snapshot(reader);
  REQUIRE(decoded);
  CHECK(reader.remaining() == 0);

  auto restored_footprint = dross::EdgeFootprint::create({edge(0, 1)}).value();
  dross::DoorRuntime restored{door_entity(), std::move(restored_footprint),
                              dross::DoorState::closed};
  REQUIRE(restored.restore(*decoded));
  CHECK(restored.snapshot() == original.snapshot());
  CHECK(restored.allows(edge(0, 1)));
}
