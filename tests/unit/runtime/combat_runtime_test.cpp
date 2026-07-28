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
