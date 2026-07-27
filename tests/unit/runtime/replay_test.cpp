#include <dross/random/random_hub.hpp>
#include <dross/runtime/replay.hpp>
#include <dross/world/world_storage.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>

namespace {

dross::ContentId content_id(const char* value) { return dross::ContentId::parse(value).value(); }

dross::CanonicalCheckpoint checkpoint_for(const std::vector<std::uint64_t>& spawn_order,
                                          const std::uint64_t seed) {
  dross::WorldStorage world{
      dross::WorldConfig{.lineage = 7, .instance_id = dross::WorldInstanceId{9}}};
  for (const auto sequence : spawn_order) {
    REQUIRE(world.write().spawn(dross::SpawnPlan::authored(sequence)));
  }
  dross::OccupancyIndex occupancy;
  dross::RandomHub random{dross::MasterSeed{seed}};
  static_cast<void>(random.stream(dross::RandomStreamId{content_id("dross:combat")}).next_u64());
  dross::NullMachineTrace trace;
  dross::WorldLifecycle lifecycle{trace};
  dross::SimulationMode mode{trace};
  REQUIRE(lifecycle.restore(
      dross::WorldLifecycleSnapshot{.state = dross::WorldLifecycleState::running}));
  return dross::canonical_checkpoint(dross::Tick{3}, world, occupancy, random.snapshot(),
                                     lifecycle.snapshot(), mode.snapshot(), {});
}

} // namespace

TEST_CASE("canonical checkpoint is independent from storage insertion order") {
  CHECK(checkpoint_for({2, 1}, 12345) == checkpoint_for({1, 2}, 12345));
}

TEST_CASE("changed master seed changes the random checkpoint section") {
  const auto first = checkpoint_for({1, 2}, 12345);
  const auto second = checkpoint_for({1, 2}, 54321);

  CHECK(first.overall != second.overall);
  CHECK(first.sections.at(dross::CheckpointSection::random) !=
        second.sections.at(dross::CheckpointSection::random));
}

TEST_CASE("machine state changes the canonical machine section") {
  dross::WorldStorage world{
      dross::WorldConfig{.lineage = 7, .instance_id = dross::WorldInstanceId{9}}};
  dross::OccupancyIndex occupancy;
  dross::RandomHub random{dross::MasterSeed{12345}};
  dross::NullMachineTrace trace;
  dross::WorldLifecycle lifecycle{trace};
  dross::SimulationMode mode{trace};
  REQUIRE(lifecycle.restore(
      dross::WorldLifecycleSnapshot{.state = dross::WorldLifecycleState::running}));
  const auto exploration =
      dross::canonical_checkpoint(dross::Tick{0}, world, occupancy, random.snapshot(),
                                  lifecycle.snapshot(), mode.snapshot(), {});
  REQUIRE(mode.request_combat());
  const auto pending =
      dross::canonical_checkpoint(dross::Tick{0}, world, occupancy, random.snapshot(),
                                  lifecycle.snapshot(), mode.snapshot(), {});

  CHECK(exploration.sections.at(dross::CheckpointSection::machines) !=
        pending.sections.at(dross::CheckpointSection::machines));
}

TEST_CASE("replay DTO has a deterministic round trip") {
  dross::ReplayLog log{
      .header =
          dross::ReplayHeader{
              .engine_version = dross::SemanticVersion{.major = 0, .minor = 1, .patch = 0},
              .schema_version = 1,
              .scenario = content_id("dross:command_event_kernel"),
              .base_package = content_id("dross:base"),
              .master_seed = dross::MasterSeed{12345},
              .random_algorithm_version = dross::random_algorithm_version,
          },
      .external_commands = {},
      .machine_trace = {},
      .checkpoints = {checkpoint_for({1, 2}, 12345)},
  };

  const auto encoded = dross::encode_replay(log);
  const auto decoded = dross::decode_replay(encoded);
  REQUIRE(decoded);
  CHECK(*decoded == log);
  CHECK(dross::encode_replay(*decoded) == encoded);
}

TEST_CASE("first replay divergence reports its tick and canonical section") {
  auto expected = checkpoint_for({1, 2}, 12345);
  auto actual = expected;
  actual.sections[dross::CheckpointSection::occupancy][0] ^= std::uint8_t{1};
  actual.overall[0] ^= std::uint8_t{1};

  const std::array expected_values{expected};
  const std::array actual_values{actual};
  const auto divergence = dross::first_divergence(expected_values, actual_values);
  REQUIRE(divergence);
  CHECK(divergence->tick == dross::Tick{3});
  CHECK(divergence->section == dross::CheckpointSection::occupancy);
}
