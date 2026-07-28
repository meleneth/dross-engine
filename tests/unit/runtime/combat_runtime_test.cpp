#include <dross/runtime/combat_runtime.hpp>

#include <catch2/catch_test_macros.hpp>

#include <vector>

namespace {

dross::CombatantDefinition combatant(const std::uint64_t sequence, const std::int32_t initiative,
                                     const std::uint32_t action_points) {
  return {
      .entity = dross::EntityRef{dross::WorldInstanceId{1}, dross::EntityId{7, sequence}},
      .initiative = initiative,
      .maximum_action_points = action_points,
  };
}

dross::ContentId id(const char* value) { return dross::ContentId::parse(value).value(); }

dross::HexPose pose(const std::int32_t q, const std::int32_t r = 0) {
  return {
      .anchor =
          {
              .region = dross::RegionId{id("demo:arena")},
              .coord = {.q = q, .r = r},
              .layer = 0,
          },
      .facing = dross::HexFacing::east,
  };
}

dross::AbilityDefinition thump() {
  return {
      .id = id("dross_demo:thump"),
      .range = 1,
      .action_point_cost = 2,
      .damage = dross::HitPoints{3},
      .presentation_cue = id("dross_demo:thump"),
  };
}

} // namespace

TEST_CASE("combat initiative is descending with a stable entity tie breaker") {
  dross::CombatSession session{{combatant(3, 8, 2), combatant(2, 12, 3), combatant(1, 12, 4)}};

  REQUIRE(session.start());
  CHECK(session.turn_order() ==
        std::vector{dross::EntityId{7, 1}, dross::EntityId{7, 2}, dross::EntityId{7, 3}});
  CHECK(session.active_actor() == dross::EntityId{7, 1});
  CHECK(session.action_points(dross::EntityId{7, 1}) == 4);
}

TEST_CASE("only the active living actor spends AP and turn start refreshes it") {
  dross::CombatSession session{{combatant(1, 10, 3), combatant(2, 5, 2)}};
  REQUIRE(session.start());

  CHECK_FALSE(session.spend_action_points(dross::EntityId{7, 2}, 1));
  CHECK(session.action_points(dross::EntityId{7, 2}) == 0);
  REQUIRE(session.spend_action_points(dross::EntityId{7, 1}, 2));
  CHECK(session.action_points(dross::EntityId{7, 1}) == 1);
  CHECK_FALSE(session.spend_action_points(dross::EntityId{7, 1}, 2));

  REQUIRE(session.end_turn(dross::EntityId{7, 1}));
  CHECK(session.active_actor() == dross::EntityId{7, 2});
  CHECK(session.action_points(dross::EntityId{7, 2}) == 2);
  REQUIRE(session.end_turn(dross::EntityId{7, 2}));
  CHECK(session.active_actor() == dross::EntityId{7, 1});
  CHECK(session.action_points(dross::EntityId{7, 1}) == 3);
}

TEST_CASE("dead combatants are skipped and one survivor completes combat") {
  dross::CombatSession session{{combatant(1, 10, 3), combatant(2, 8, 3), combatant(3, 6, 3)}};
  REQUIRE(session.start());
  REQUIRE(session.set_alive(dross::EntityId{7, 2}, false));
  REQUIRE(session.end_turn(dross::EntityId{7, 1}));
  CHECK(session.active_actor() == dross::EntityId{7, 3});

  REQUIRE(session.set_alive(dross::EntityId{7, 1}, false));
  CHECK(session.state() == dross::CombatSessionState::completed);
  CHECK(session.active_actor() == dross::EntityId{7, 3});
  CHECK_FALSE(session.end_turn(dross::EntityId{7, 3}));
}

TEST_CASE("generic adjacent ability spends AP and commits deterministic damage") {
  dross::CombatSession session{{combatant(1, 10, 3), combatant(2, 5, 2)}};
  REQUIRE(session.start());
  dross::AbilityResolver resolver{
      session,
      {
          {.entity = combatant(1, 10, 3).entity, .pose = pose(0), .health = dross::HitPoints{8}},
          {.entity = combatant(2, 5, 2).entity, .pose = pose(1), .health = dross::HitPoints{5}},
      },
  };

  const auto result = resolver.perform(thump(), dross::EntityId{7, 1}, dross::EntityId{7, 2});
  CHECK(result.accepted);
  CHECK(result.damage == dross::HitPoints{3});
  CHECK(result.remaining_health == dross::HitPoints{2});
  CHECK_FALSE(result.killed);
  CHECK(session.action_points(dross::EntityId{7, 1}) == 1);
  CHECK(resolver.health(dross::EntityId{7, 2}) == dross::HitPoints{2});
}

TEST_CASE("rejected ability leaves AP and health unchanged") {
  dross::CombatSession session{{combatant(1, 10, 3), combatant(2, 5, 2)}};
  REQUIRE(session.start());
  dross::AbilityResolver resolver{
      session,
      {
          {.entity = combatant(1, 10, 3).entity, .pose = pose(0), .health = dross::HitPoints{8}},
          {.entity = combatant(2, 5, 2).entity, .pose = pose(2), .health = dross::HitPoints{5}},
      },
  };

  const auto result = resolver.perform(thump(), dross::EntityId{7, 1}, dross::EntityId{7, 2});
  CHECK_FALSE(result.accepted);
  CHECK(result.rejection == dross::AbilityRejection::out_of_range);
  CHECK(session.action_points(dross::EntityId{7, 1}) == 3);
  CHECK(resolver.health(dross::EntityId{7, 2}) == dross::HitPoints{5});
}

TEST_CASE("lethal generic ability marks the target dead and completes combat") {
  dross::CombatSession session{{combatant(1, 10, 3), combatant(2, 5, 2)}};
  REQUIRE(session.start());
  dross::AbilityResolver resolver{
      session,
      {
          {.entity = combatant(1, 10, 3).entity, .pose = pose(0), .health = dross::HitPoints{8}},
          {.entity = combatant(2, 5, 2).entity, .pose = pose(1), .health = dross::HitPoints{3}},
      },
  };

  const auto result = resolver.perform(thump(), dross::EntityId{7, 1}, dross::EntityId{7, 2});
  CHECK(result.accepted);
  CHECK(result.killed);
  CHECK(result.remaining_health == dross::HitPoints{0});
  CHECK(session.state() == dross::CombatSessionState::completed);
}
