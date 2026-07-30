#include <dross/runtime/combat_runtime.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string>
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

class RecordingCombatEvents final : public dross::AbilityResolver::EventSink {
public:
  void publish(const dross::combat::AbilityCommitted& event) override {
    calls.emplace_back("ability");
    ability = event;
  }
  void publish(const dross::combat::DamageApplied& event) override {
    calls.emplace_back("damage");
    damage = event;
  }
  void publish(const dross::combat::ActorKilled& event) override {
    calls.emplace_back("killed");
    killed = event;
  }

  std::vector<std::string> calls;
  std::optional<dross::combat::AbilityCommitted> ability;
  std::optional<dross::combat::DamageApplied> damage;
  std::optional<dross::combat::ActorKilled> killed;
};

class RecordingSessionEvents final : public dross::CombatSession::EventSink {
public:
  void publish(const dross::combat::CombatStarted& event) override {
    calls.emplace_back("combat:" + std::to_string(event.active_actor.id().sequence()));
  }
  void publish(const dross::combat::TurnStarted& event) override {
    calls.emplace_back("turn:" + std::to_string(event.actor.id().sequence()) + ":" +
                       std::to_string(event.action_points));
  }
  void publish(const dross::combat::ActionPointsSpent& event) override {
    calls.emplace_back("spent:" + std::to_string(event.amount) + ":" +
                       std::to_string(event.remaining));
  }

  std::vector<std::string> calls;
};

class RejectingAbilityRule final : public dross::AbilityResolver::RuleSource {
public:
  bool allows(const dross::AbilityDefinition&, const dross::EntityRef&,
              const dross::EntityRef&) override {
    ++calls;
    return false;
  }

  std::uint32_t calls{0};
};

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

TEST_CASE("typed end turn validates the complete actor reference") {
  dross::CombatSession session{{combatant(1, 10, 3), combatant(2, 5, 2)}};
  REQUIRE(session.start());
  const dross::EntityRef foreign{dross::WorldInstanceId{99}, dross::EntityId{7, 1}};

  const auto rejected = session.handle(dross::combat::EndTurn{.actor = foreign});
  REQUIRE_FALSE(rejected);
  CHECK(rejected.error() == dross::CombatCommandRejection::wrong_entity);
  CHECK(session.active_actor() == dross::EntityId{7, 1});
  REQUIRE(session.handle(dross::combat::EndTurn{.actor = combatant(1, 10, 3).entity}));
}

TEST_CASE("combat lifecycle emits typed facts only after committed transitions") {
  RecordingSessionEvents events;
  dross::CombatSession session{{combatant(1, 10, 3), combatant(2, 5, 2)}, &events};

  CHECK_FALSE(session.spend_action_points(dross::EntityId{7, 1}, 1));
  CHECK(events.calls.empty());
  REQUIRE(session.start());
  CHECK(events.calls == std::vector<std::string>{"combat:1", "turn:1:3"});
  REQUIRE(session.spend_action_points(dross::EntityId{7, 1}, 2));
  CHECK(events.calls == std::vector<std::string>{"combat:1", "turn:1:3", "spent:2:1"});
  REQUIRE(session.end_turn(dross::EntityId{7, 1}));
  CHECK(events.calls == std::vector<std::string>{"combat:1", "turn:1:3", "spent:2:1", "turn:2:2"});
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

TEST_CASE("combat session snapshot codec restores active turn AP and living roster") {
  dross::CombatSession original{{combatant(1, 10, 4), combatant(2, 8, 3), combatant(3, 6, 2)}};
  REQUIRE(original.start());
  REQUIRE(original.spend_action_points(dross::EntityId{7, 1}, 2));
  REQUIRE(original.set_alive(dross::EntityId{7, 2}, false));

  dross::ByteWriter writer;
  dross::encode_combat_session_snapshot(writer, original.snapshot());
  dross::ByteReader reader{writer.bytes()};
  const auto decoded = dross::decode_combat_session_snapshot(reader);
  REQUIRE(decoded);
  CHECK(reader.remaining() == 0);

  const auto restored = dross::CombatSession::from_snapshot(*decoded, dross::WorldInstanceId{42});
  REQUIRE(restored);
  CHECK((*restored)->snapshot() == original.snapshot());
  REQUIRE((*restored)->end_turn(dross::EntityId{7, 1}));
  CHECK((*restored)->active_actor() == dross::EntityId{7, 3});
  CHECK((*restored)->action_points(dross::EntityId{7, 3}) == 2);

  auto mismatched = *decoded;
  mismatched.combatants[0].entity = dross::EntityId{99, 1};
  dross::CombatSession rejected{{combatant(1, 10, 4), combatant(2, 8, 3), combatant(3, 6, 2)}};
  CHECK_FALSE(rejected.restore(mismatched));
  CHECK(rejected.state() == dross::CombatSessionState::inactive);

  auto invalid = *decoded;
  invalid.combatants[0].action_points = invalid.combatants[0].maximum_action_points + 1;
  CHECK_FALSE(dross::CombatSession::from_snapshot(invalid, dross::WorldInstanceId{42}));
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

TEST_CASE("typed ability request validates stable ability and entity references") {
  dross::CombatSession session{{combatant(1, 10, 3), combatant(2, 5, 2)}};
  REQUIRE(session.start());
  dross::AbilityResolver resolver{
      session,
      {
          {.entity = combatant(1, 10, 3).entity, .pose = pose(0), .health = dross::HitPoints{8}},
          {.entity = combatant(2, 5, 2).entity, .pose = pose(1), .health = dross::HitPoints{5}},
      },
  };
  const dross::EntityRef foreign_actor{dross::WorldInstanceId{99}, dross::EntityId{7, 1}};

  const auto rejected = resolver.perform(
      thump(),
      {.actor = foreign_actor, .target = combatant(2, 5, 2).entity, .ability = thump().id});
  CHECK_FALSE(rejected.accepted);
  CHECK(rejected.rejection == dross::AbilityRejection::invalid_ability);
  CHECK(session.action_points(dross::EntityId{7, 1}) == 3);
  CHECK(resolver.health(dross::EntityId{7, 2}) == dross::HitPoints{5});

  const auto accepted = resolver.perform(thump(), {.actor = combatant(1, 10, 3).entity,
                                                   .target = combatant(2, 5, 2).entity,
                                                   .ability = thump().id});
  CHECK(accepted.accepted);
}

TEST_CASE("ability resolver snapshot codec restores authoritative pose and health") {
  dross::CombatSession session{{combatant(1, 10, 3), combatant(2, 5, 2)}};
  REQUIRE(session.start());
  dross::AbilityResolver original{
      session,
      {
          {.entity = combatant(1, 10, 3).entity, .pose = pose(0), .health = dross::HitPoints{8}},
          {.entity = combatant(2, 5, 2).entity, .pose = pose(1), .health = dross::HitPoints{5}},
      },
  };
  REQUIRE(original.perform(thump(), dross::EntityId{7, 1}, dross::EntityId{7, 2}).accepted);

  dross::ByteWriter writer;
  dross::encode_ability_resolver_snapshot(writer, original.snapshot());
  dross::ByteReader reader{writer.bytes()};
  const auto decoded = dross::decode_ability_resolver_snapshot(reader);
  REQUIRE(decoded);
  CHECK(reader.remaining() == 0);
  const auto restored =
      dross::AbilityResolver::from_snapshot(session, *decoded, dross::WorldInstanceId{42});
  REQUIRE(restored);
  CHECK((*restored)->snapshot() == original.snapshot());
  CHECK((*restored)->health(dross::EntityId{7, 2}) == dross::HitPoints{2});

  auto duplicate = *decoded;
  duplicate.actors.push_back(duplicate.actors.front());
  CHECK_FALSE(
      dross::AbilityResolver::from_snapshot(session, duplicate, dross::WorldInstanceId{42}));
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

TEST_CASE("pre-resolution ability rule rejects before AP damage or events") {
  dross::CombatSession session{{combatant(1, 10, 3), combatant(2, 5, 2)}};
  REQUIRE(session.start());
  RecordingCombatEvents events;
  RejectingAbilityRule rules;
  dross::AbilityResolver resolver{
      session,
      {
          {.entity = combatant(1, 10, 3).entity, .pose = pose(0), .health = dross::HitPoints{8}},
          {.entity = combatant(2, 5, 2).entity, .pose = pose(1), .health = dross::HitPoints{5}},
      },
      &events,
      nullptr,
      &rules,
  };

  const auto result = resolver.perform(thump(), dross::EntityId{7, 1}, dross::EntityId{7, 2});
  CHECK_FALSE(result.accepted);
  CHECK(result.rejection == dross::AbilityRejection::rule_rejected);
  CHECK(rules.calls == 1);
  CHECK(session.action_points(dross::EntityId{7, 1}) == 3);
  CHECK(resolver.health(dross::EntityId{7, 2}) == dross::HitPoints{5});
  CHECK(events.calls.empty());
}

TEST_CASE("lethal generic ability marks the target dead and completes combat") {
  dross::CombatSession session{{combatant(1, 10, 3), combatant(2, 5, 2)}};
  REQUIRE(session.start());
  RecordingCombatEvents events;
  dross::AbilityResolver resolver{
      session,
      {
          {.entity = combatant(1, 10, 3).entity, .pose = pose(0), .health = dross::HitPoints{8}},
          {.entity = combatant(2, 5, 2).entity, .pose = pose(1), .health = dross::HitPoints{3}},
      },
      &events,
  };

  const auto result = resolver.perform(thump(), dross::EntityId{7, 1}, dross::EntityId{7, 2});
  CHECK(result.accepted);
  CHECK(result.killed);
  CHECK(result.remaining_health == dross::HitPoints{0});
  CHECK(session.state() == dross::CombatSessionState::completed);
  CHECK(events.calls == std::vector<std::string>{"ability", "damage", "killed"});
  REQUIRE(events.ability);
  CHECK(events.ability->ability == id("dross_demo:thump"));
  REQUIRE(events.damage);
  CHECK(events.damage->amount == dross::HitPoints{3});
  CHECK(events.damage->target == combatant(2, 5, 2).entity);
  REQUIRE(events.killed);
  CHECK(events.killed->ability == id("dross_demo:thump"));
}

TEST_CASE("seeded bonus damage is reproducible through a named RandomHub stream") {
  auto resolve = [](const std::uint64_t seed) {
    dross::CombatSession session{{combatant(1, 10, 3), combatant(2, 5, 2)}};
    REQUIRE(session.start());
    dross::RandomHub random{dross::MasterSeed{seed}};
    auto& stream = random.stream(dross::RandomStreamId{id("dross:combat_damage")});
    dross::AbilityResolver resolver{
        session,
        {
            {.entity = combatant(1, 10, 3).entity, .pose = pose(0), .health = dross::HitPoints{8}},
            {.entity = combatant(2, 5, 2).entity, .pose = pose(1), .health = dross::HitPoints{20}},
        },
        nullptr,
        &stream,
    };
    auto ability = thump();
    ability.bonus_damage_max = 3;
    return resolver.perform(ability, dross::EntityId{7, 1}, dross::EntityId{7, 2}).damage;
  };

  CHECK(resolve(12345) == resolve(12345));
  CHECK(resolve(12345).value() >= 3);
  CHECK(resolve(12345).value() <= 6);
}
