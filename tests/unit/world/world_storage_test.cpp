#include <dross/world/world_storage.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <string_view>

namespace {

dross::EntityAlias alias(const std::string_view text) {
  return dross::EntityAlias{dross::ContentId::parse(text).value()};
}

dross::WorldStorage make_world(const std::uint64_t instance = 99) {
  return dross::WorldStorage{
      dross::WorldConfig{.lineage = 17, .instance_id = dross::WorldInstanceId{instance}}};
}

} // namespace

TEST_CASE("runtime entity allocation is deterministic and monotonic") {
  auto world = make_world();
  auto write = world.write();

  const auto first = write.spawn(dross::SpawnPlan::runtime());
  const auto second = write.spawn(dross::SpawnPlan::runtime());

  REQUIRE(first);
  REQUIRE(second);
  CHECK(first->id() == dross::EntityId{17, 1});
  CHECK(second->id() == dross::EntityId{17, 2});
  CHECK(world.allocator_snapshot().next_runtime_sequence == 3);
}

TEST_CASE("authored identity and aliases reject duplicates atomically") {
  auto world = make_world();
  auto write = world.write();

  REQUIRE(write.spawn(dross::SpawnPlan::authored(50, alias("demo:first"))));
  const auto count_before = world.read().entity_count();

  const auto duplicate_id = write.spawn(dross::SpawnPlan::authored(50, alias("demo:second")));
  REQUIRE_FALSE(duplicate_id);
  CHECK(duplicate_id.error().reason == dross::SpawnErrorReason::duplicate_id);

  const auto duplicate_alias = write.spawn(dross::SpawnPlan::authored(51, alias("demo:first")));
  REQUIRE_FALSE(duplicate_alias);
  CHECK(duplicate_alias.error().reason == dross::SpawnErrorReason::duplicate_alias);
  CHECK(world.read().entity_count() == count_before);
  CHECK_FALSE(world.read().find(dross::EntityId{17, 51}));
}

TEST_CASE("entities can be queried by stable ID and alias") {
  auto world = make_world();
  auto write = world.write();
  const auto spawned = write.spawn(dross::SpawnPlan::authored(8, alias("demo:named_actor")));
  REQUIRE(spawned);

  CHECK(world.read().find(spawned->id()) == *spawned);
  CHECK(world.read().find(alias("demo:named_actor")) == *spawned);
  REQUIRE(world.read().identity(*spawned));
  CHECK(world.read().identity(*spawned)->alias == alias("demo:named_actor"));
}

TEST_CASE("world instance validation rejects foreign and stale references") {
  auto world = make_world();
  auto write = world.write();
  const auto spawned = write.spawn(dross::SpawnPlan::runtime());
  REQUIRE(spawned);

  const dross::EntityRef foreign{dross::WorldInstanceId{100}, spawned->id()};
  CHECK_FALSE(world.read().valid(foreign));
  CHECK(world.read().lookup(foreign).error().reason ==
        dross::EntityLookupErrorReason::wrong_world_instance);

  REQUIRE(write.destroy(*spawned));
  CHECK_FALSE(world.read().valid(*spawned));
  CHECK(world.read().lookup(*spawned).error().reason ==
        dross::EntityLookupErrorReason::entity_not_found);
}

TEST_CASE("destroyed IDs and aliases are not reused implicitly") {
  auto world = make_world();
  auto write = world.write();
  const auto first = write.spawn(dross::SpawnPlan::runtime(alias("demo:temporary")));
  REQUIRE(first);
  REQUIRE(write.destroy(*first));

  const auto second = write.spawn(dross::SpawnPlan::runtime());
  REQUIRE(second);
  CHECK(second->id() == dross::EntityId{17, 2});
  CHECK_FALSE(world.read().find(alias("demo:temporary")));

  const auto reused_id = write.spawn(dross::SpawnPlan::authored(1, alias("demo:replacement")));
  REQUIRE_FALSE(reused_id);
  CHECK(reused_id.error().reason == dross::SpawnErrorReason::retired_id);
}

TEST_CASE("stable identity iteration is independent of EnTT storage order") {
  auto first_world = make_world(1);
  auto second_world = make_world(2);

  REQUIRE(first_world.write().spawn(dross::SpawnPlan::authored(30)));
  REQUIRE(first_world.write().spawn(dross::SpawnPlan::authored(10)));
  REQUIRE(first_world.write().spawn(dross::SpawnPlan::authored(20)));

  REQUIRE(second_world.write().spawn(dross::SpawnPlan::authored(20)));
  REQUIRE(second_world.write().spawn(dross::SpawnPlan::authored(30)));
  REQUIRE(second_world.write().spawn(dross::SpawnPlan::authored(10)));

  const auto first_ids = first_world.read().stable_entity_ids();
  const auto second_ids = second_world.read().stable_entity_ids();
  CHECK(first_ids == second_ids);
  CHECK(std::ranges::is_sorted(first_ids));
}

TEST_CASE("direct iteration does not promise stable identity order") {
  auto world = make_world();
  REQUIRE(world.write().spawn(dross::SpawnPlan::authored(30)));
  REQUIRE(world.write().spawn(dross::SpawnPlan::authored(10)));

  const auto direct = world.read().entity_ids();
  const auto stable = world.read().stable_entity_ids();
  CHECK(std::ranges::is_sorted(stable));
  CHECK(direct.size() == stable.size());
}
