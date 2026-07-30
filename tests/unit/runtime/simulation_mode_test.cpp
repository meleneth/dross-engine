#include <dross/runtime/simulation_mode.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>

TEST_CASE("combat begins only after a safe fixed-tick boundary") {
  dross::InMemoryMachineTrace trace;
  dross::SimulationMode mode{trace};

  CHECK(mode.state() == dross::SimulationModeState::exploration);
  CHECK(mode.request_combat());
  CHECK(mode.state() == dross::SimulationModeState::combat_pending);
  CHECK(mode.reach_safe_boundary());
  CHECK(mode.state() == dross::SimulationModeState::combat);
  CHECK(mode.end_combat());
  CHECK(mode.state() == dross::SimulationModeState::exploration);
}

TEST_CASE("duplicate combat requests are rejected and traced") {
  dross::InMemoryMachineTrace trace;
  dross::SimulationMode mode{trace};
  REQUIRE(mode.request_combat());

  CHECK_FALSE(mode.request_combat());
  REQUIRE(trace.entries().size() == 2);
  CHECK(trace.entries().back().machine == dross::MachineFamily::simulation_mode);
  CHECK(trace.entries().back().event == dross::MachineEventId::combat_requested);
  CHECK(trace.entries().back().outcome == dross::MachineEventOutcome::rejected);
  CHECK(trace.entries().back().source == dross::MachineStateId::combat_pending);
  CHECK(trace.entries().back().destination == dross::MachineStateId::combat_pending);
}

TEST_CASE("typed combat request validates participants before changing mode") {
  dross::InMemoryMachineTrace trace;
  dross::SimulationMode mode{trace};
  const dross::EntityRef player{dross::WorldInstanceId{1}, dross::EntityId{7, 1}};
  const dross::EntityRef foreign{dross::WorldInstanceId{2}, dross::EntityId{7, 2}};

  const auto rejected =
      mode.handle(dross::combat::RequestCombatStart{.requester = player, .opponent = foreign});
  REQUIRE_FALSE(rejected);
  CHECK(rejected.error() == dross::SimulationModeCommandRejection::invalid_participants);
  CHECK(mode.state() == dross::SimulationModeState::exploration);
  CHECK(trace.entries().empty());

  REQUIRE(mode.handle(dross::combat::RequestCombatStart{
      .requester = player,
      .opponent = dross::EntityRef{dross::WorldInstanceId{1}, dross::EntityId{7, 2}},
  }));
  CHECK(mode.state() == dross::SimulationModeState::combat_pending);
}

TEST_CASE("safe boundary outside combat pending is visibly rejected") {
  dross::InMemoryMachineTrace trace;
  dross::SimulationMode mode{trace};

  CHECK_FALSE(mode.reach_safe_boundary());
  REQUIRE(trace.entries().size() == 1);
  CHECK(trace.entries().front().event == dross::MachineEventId::safe_boundary_reached);
  CHECK(trace.entries().front().outcome == dross::MachineEventOutcome::rejected);
}

TEST_CASE("simulation mode restores every persistent state through production events") {
  constexpr std::array states{
      dross::SimulationModeState::exploration,
      dross::SimulationModeState::combat_pending,
      dross::SimulationModeState::combat,
  };

  for (const auto state : states) {
    dross::NullMachineTrace trace;
    dross::SimulationMode mode{trace};
    REQUIRE(mode.restore(dross::SimulationModeSnapshot{.state = state}));
    CHECK(mode.snapshot().state == state);
  }
}
