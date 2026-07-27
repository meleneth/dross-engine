#include <dross/runtime/world_lifecycle.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>

TEST_CASE("world lifecycle follows every legal load run save and unload transition") {
  dross::InMemoryMachineTrace trace;
  dross::WorldLifecycle lifecycle{trace};

  CHECK(lifecycle.state() == dross::WorldLifecycleState::empty);
  CHECK(lifecycle.begin_load());
  CHECK(lifecycle.state() == dross::WorldLifecycleState::loading);
  CHECK(lifecycle.load_succeeded());
  CHECK(lifecycle.state() == dross::WorldLifecycleState::ready);
  CHECK(lifecycle.begin_run());
  CHECK(lifecycle.state() == dross::WorldLifecycleState::running);
  CHECK(lifecycle.begin_save());
  CHECK(lifecycle.state() == dross::WorldLifecycleState::saving);
  CHECK(lifecycle.save_succeeded());
  CHECK(lifecycle.state() == dross::WorldLifecycleState::running);
  CHECK(lifecycle.begin_unload());
  CHECK(lifecycle.state() == dross::WorldLifecycleState::unloading);
  CHECK(lifecycle.unload_succeeded());
  CHECK(lifecycle.state() == dross::WorldLifecycleState::empty);
}

TEST_CASE("recoverable save failure preserves the running world") {
  dross::NullMachineTrace trace;
  dross::WorldLifecycle lifecycle{trace};
  REQUIRE(lifecycle.begin_load());
  REQUIRE(lifecycle.load_succeeded());
  REQUIRE(lifecycle.begin_run());
  REQUIRE(lifecycle.begin_save());

  CHECK(lifecycle.save_io_failed());
  CHECK(lifecycle.state() == dross::WorldLifecycleState::running);
}

TEST_CASE("load failure and fatal faults enter an absorbing faulted state") {
  dross::NullMachineTrace trace;
  dross::WorldLifecycle load_failure{trace};
  REQUIRE(load_failure.begin_load());
  REQUIRE(load_failure.load_failed());
  CHECK(load_failure.state() == dross::WorldLifecycleState::faulted);
  CHECK_FALSE(load_failure.begin_load());

  dross::WorldLifecycle fatal{trace};
  REQUIRE(fatal.fatal_fault());
  CHECK(fatal.state() == dross::WorldLifecycleState::faulted);
  CHECK_FALSE(fatal.begin_save());
}

TEST_CASE("unexpected lifecycle events are rejected and traced") {
  dross::InMemoryMachineTrace trace;
  dross::WorldLifecycle lifecycle{trace};

  CHECK_FALSE(lifecycle.begin_run());
  REQUIRE(trace.entries().size() == 1);
  CHECK(trace.entries().front().machine == dross::MachineFamily::world_lifecycle);
  CHECK(trace.entries().front().event == dross::MachineEventId::begin_run);
  CHECK(trace.entries().front().outcome == dross::MachineEventOutcome::rejected);
  CHECK(trace.entries().front().source == dross::MachineStateId::world_empty);
  CHECK(trace.entries().front().destination == dross::MachineStateId::world_empty);
}

TEST_CASE("world lifecycle restores every persistent state through production events") {
  constexpr std::array states{
      dross::WorldLifecycleState::empty,     dross::WorldLifecycleState::loading,
      dross::WorldLifecycleState::ready,     dross::WorldLifecycleState::running,
      dross::WorldLifecycleState::saving,    dross::WorldLifecycleState::unloading,
      dross::WorldLifecycleState::faulted,
  };

  for (const auto state : states) {
    dross::NullMachineTrace trace;
    dross::WorldLifecycle lifecycle{trace};
    REQUIRE(lifecycle.restore(dross::WorldLifecycleSnapshot{.state = state}));
    CHECK(lifecycle.snapshot().state == state);
  }
}
