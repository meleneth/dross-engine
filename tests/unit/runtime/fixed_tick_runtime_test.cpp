#include <dross/runtime/fixed_tick_runtime.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace {

dross::ContentId fixed_tick_id(const char* value) { return dross::ContentId::parse(value).value(); }

dross::HexCellId fixed_tick_cell(const int column) {
  return dross::HexCellId{
      .region = dross::RegionId{fixed_tick_id("test:runtime")},
      .coord = dross::HexCoord{.q = column, .r = 0},
      .layer = 0,
  };
}

dross::CompiledHexMap fixed_tick_map(const int cell_count) {
  dross::CompiledHexMapBuilder builder;
  for (int column = 0; column < cell_count; ++column) {
    REQUIRE(builder
                .add_cell(dross::CellFacts{
                    .id = fixed_tick_cell(column),
                    .surface_height = dross::Millimeters{0},
                    .terrain = fixed_tick_id("test:floor"),
                    .base_cost = dross::MovementCost{1},
                    .clearance = dross::Clearance::open,
                    .traversable = true,
                    .semantic_tags = {},
                })
                .has_value());
  }
  return std::move(builder).build().value();
}

dross::PlaceEntityEnvelope scheduled_command(const std::uint64_t command_id, const dross::Tick tick,
                                             const dross::EntityRef entity, const int column) {
  return dross::PlaceEntityEnvelope{
      .metadata =
          dross::CommandMetadata{
              .id = dross::CommandId{command_id},
              .tick = tick,
              .source = dross::CommandSource::headless_test,
              .causation = dross::CausationId{command_id + 100},
              .correlation = dross::CorrelationId{1},
          },
      .payload =
          dross::placement::PlaceEntity{
              .entity = entity,
              .target =
                  dross::HexPose{
                      .anchor = fixed_tick_cell(column),
                      .facing = dross::HexFacing::east,
                  },
          },
  };
}

struct RuntimeFixture {
  explicit RuntimeFixture(const int cell_count)
      : world{dross::WorldConfig{
            .lineage = 41,
            .instance_id = dross::WorldInstanceId{12},
        }},
        map{fixed_tick_map(cell_count)}, lifecycle{machine_trace}, mode{machine_trace} {
    REQUIRE(lifecycle.begin_load());
    REQUIRE(lifecycle.load_succeeded());
    REQUIRE(lifecycle.begin_run());
  }

  dross::EntityRef spawn() { return world.write().spawn(dross::SpawnPlan::runtime()).value(); }

  dross::WorldStorage world;
  dross::CompiledHexMap map;
  dross::HeadlessPlacementScriptPort scripts;
  dross::InMemoryTraceSink trace;
  dross::NullMachineTrace machine_trace;
  dross::WorldLifecycle lifecycle;
  dross::SimulationMode mode;
};

} // namespace

TEST_CASE("simulation clock advances only by explicit fixed ticks") {
  dross::SimulationClock clock{dross::Tick{7}, 30};

  CHECK(clock.current() == dross::Tick{7});
  CHECK(clock.ticks_per_second() == 30);
  REQUIRE(clock.advance());
  CHECK(clock.current() == dross::Tick{8});
}

TEST_CASE("external commands are ingested by target tick in submission order") {
  RuntimeFixture fixture{2};
  const auto first = fixture.spawn();
  const auto second = fixture.spawn();
  dross::CommandEventKernel kernel{fixture.world, std::move(fixture.map), fixture.scripts,
                                   fixture.trace};
  dross::EngineRuntime runtime{kernel, fixture.lifecycle, fixture.mode, dross::RuntimeConfig{}};
  REQUIRE(runtime.schedule_external(scheduled_command(2, dross::Tick{1}, second, 1)));
  REQUIRE(runtime.schedule_external(scheduled_command(1, dross::Tick{1}, first, 0)));

  const auto tick_zero = runtime.advance_tick();
  const auto tick_one = runtime.advance_tick();

  CHECK(tick_zero.command_results.empty());
  REQUIRE(tick_one.command_results.size() == 2);
  CHECK(tick_one.command_ids ==
        std::vector<dross::CommandId>{dross::CommandId{2}, dross::CommandId{1}});
  CHECK(fixture.world.read().pose(first).has_value());
  CHECK(fixture.world.read().pose(second).has_value());
}

TEST_CASE("follow-up commands finish in bounded cycles within the same tick") {
  RuntimeFixture fixture{2};
  const auto first = fixture.spawn();
  const auto second = fixture.spawn();
  dross::CommandEventKernel kernel{fixture.world, std::move(fixture.map), fixture.scripts,
                                   fixture.trace};
  auto subscription = kernel.events().subscribe_capability(
      [&first, &second](const dross::placement::EntityPlaced& event,
                        dross::EventReactionContext& context) {
        if (event.entity == first) {
          context.enqueue_follow_up(scheduled_command(2, dross::Tick{0}, second, 1));
        }
      });
  REQUIRE(subscription);
  dross::EngineRuntime runtime{
      kernel, fixture.lifecycle, fixture.mode,
      dross::RuntimeConfig{.ticks_per_second = 30, .max_command_cycles_per_tick = 4}};
  REQUIRE(runtime.schedule_external(scheduled_command(1, dross::Tick{0}, first, 0)));

  const auto report = runtime.advance_tick();

  CHECK(report.command_cycles == 2);
  CHECK(report.command_results.size() == 2);
  CHECK(runtime.state() == dross::RuntimeState::running);
  CHECK(fixture.world.read().pose(second).has_value());
  CHECK(report.phases == std::vector<dross::TickPhase>{
                             dross::TickPhase::ingest_external,
                             dross::TickPhase::process_commands,
                             dross::TickPhase::advance_time_systems,
                             dross::TickPhase::produce_inspection,
                             dross::TickPhase::checkpoint,
                             dross::TickPhase::increment_clock,
                         });
}

TEST_CASE("command cycle budget exhaustion faults the runtime") {
  RuntimeFixture fixture{3};
  const auto first = fixture.spawn();
  const auto second = fixture.spawn();
  const auto third = fixture.spawn();
  dross::CommandEventKernel kernel{fixture.world, std::move(fixture.map), fixture.scripts,
                                   fixture.trace};
  auto subscription = kernel.events().subscribe_capability(
      [&first, &second, &third](const dross::placement::EntityPlaced& event,
                                dross::EventReactionContext& context) {
        if (event.entity == first) {
          context.enqueue_follow_up(scheduled_command(2, dross::Tick{0}, second, 1));
        } else if (event.entity == second) {
          context.enqueue_follow_up(scheduled_command(3, dross::Tick{0}, third, 2));
        }
      });
  REQUIRE(subscription);
  dross::EngineRuntime runtime{
      kernel, fixture.lifecycle, fixture.mode,
      dross::RuntimeConfig{.ticks_per_second = 30, .max_command_cycles_per_tick = 2}};
  REQUIRE(runtime.schedule_external(scheduled_command(1, dross::Tick{0}, first, 0)));

  const auto report = runtime.advance_tick();

  CHECK(runtime.state() == dross::RuntimeState::faulted);
  REQUIRE(report.fault);
  CHECK(*report.fault == dross::RuntimeFault::command_cycle_budget_exhausted);
  CHECK(report.command_cycles == 2);
  CHECK_FALSE(fixture.world.read().pose(third));
}

TEST_CASE("commands cannot be scheduled into an elapsed tick") {
  RuntimeFixture fixture{1};
  const auto entity = fixture.spawn();
  dross::CommandEventKernel kernel{fixture.world, std::move(fixture.map), fixture.scripts,
                                   fixture.trace};
  dross::EngineRuntime runtime{kernel, fixture.lifecycle, fixture.mode, dross::RuntimeConfig{}};
  static_cast<void>(runtime.advance_tick());

  const auto scheduled = runtime.schedule_external(scheduled_command(1, dross::Tick{0}, entity, 0));

  REQUIRE_FALSE(scheduled);
  CHECK(scheduled.error() == dross::ScheduleError::elapsed_tick);
}

TEST_CASE("runtime rejects commands until world lifecycle is running") {
  RuntimeFixture fixture{1};
  REQUIRE(fixture.lifecycle.begin_unload());
  REQUIRE(fixture.lifecycle.unload_succeeded());
  const auto entity = fixture.spawn();
  dross::CommandEventKernel kernel{fixture.world, std::move(fixture.map), fixture.scripts,
                                   fixture.trace};
  dross::EngineRuntime runtime{kernel, fixture.lifecycle, fixture.mode, dross::RuntimeConfig{}};

  const auto scheduled = runtime.schedule_external(scheduled_command(1, dross::Tick{0}, entity, 0));

  REQUIRE_FALSE(scheduled);
  CHECK(scheduled.error() == dross::ScheduleError::world_not_running);
}

TEST_CASE("runtime releases combat pending only at its safe tick boundary") {
  RuntimeFixture fixture{1};
  dross::CommandEventKernel kernel{fixture.world, std::move(fixture.map), fixture.scripts,
                                   fixture.trace};
  dross::EngineRuntime runtime{kernel, fixture.lifecycle, fixture.mode, dross::RuntimeConfig{}};

  REQUIRE(runtime.request_combat());
  CHECK(fixture.mode.state() == dross::SimulationModeState::combat_pending);
  static_cast<void>(runtime.advance_tick());
  CHECK(fixture.mode.state() == dross::SimulationModeState::combat);
}

TEST_CASE("fatal lifecycle fault prevents later commands") {
  RuntimeFixture fixture{1};
  const auto entity = fixture.spawn();
  dross::CommandEventKernel kernel{fixture.world, std::move(fixture.map), fixture.scripts,
                                   fixture.trace};
  dross::EngineRuntime runtime{kernel, fixture.lifecycle, fixture.mode, dross::RuntimeConfig{}};
  REQUIRE(fixture.lifecycle.fatal_fault());

  CHECK_FALSE(runtime.schedule_external(scheduled_command(1, dross::Tick{0}, entity, 0)));
  CHECK_FALSE(fixture.lifecycle.begin_save());
}

TEST_CASE("save boundary accepts exploration and combat between command phases") {
  RuntimeFixture fixture{1};
  dross::CommandEventKernel kernel{fixture.world, std::move(fixture.map), fixture.scripts,
                                   fixture.trace};
  dross::EngineRuntime runtime{kernel, fixture.lifecycle, fixture.mode, dross::RuntimeConfig{}};

  CHECK(runtime.save_boundary());
  REQUIRE(fixture.mode.restore(
      dross::SimulationModeSnapshot{.state = dross::SimulationModeState::combat}));
  CHECK(runtime.save_boundary());
}

TEST_CASE("save boundary refuses pending and active command work") {
  struct BoundaryProbeScript final : dross::PlacementScriptPort {
    dross::EngineRuntime* runtime{nullptr};
    mutable std::optional<dross::SaveBoundaryError> observed;

    dross::PlacementRuleContribution
    contribute(const dross::placement::PlaceEntity&) const override {
      const auto boundary = runtime->save_boundary();
      if (!boundary) {
        observed = boundary.error();
      }
      return dross::PlacementRuleContribution{
          .phase = dross::PlacementRulePhase::script,
          .accepted = true,
          .reason = std::nullopt,
      };
    }
  };

  RuntimeFixture fixture{1};
  const auto entity = fixture.spawn();
  BoundaryProbeScript scripts;
  dross::CommandEventKernel kernel{fixture.world, std::move(fixture.map), scripts, fixture.trace};
  dross::EngineRuntime runtime{kernel, fixture.lifecycle, fixture.mode, dross::RuntimeConfig{}};
  scripts.runtime = &runtime;
  kernel.enqueue(scheduled_command(1, dross::Tick{0}, entity, 0));

  const auto pending = runtime.save_boundary();
  REQUIRE_FALSE(pending);
  CHECK(pending.error() == dross::SaveBoundaryError::commands_pending);
  static_cast<void>(kernel.run_cycle());
  REQUIRE(scripts.observed);
  CHECK(*scripts.observed == dross::SaveBoundaryError::command_active);
}

TEST_CASE("save boundary refuses an event queue drain") {
  RuntimeFixture fixture{1};
  const auto entity = fixture.spawn();
  dross::CommandEventKernel kernel{fixture.world, std::move(fixture.map), fixture.scripts,
                                   fixture.trace};
  dross::EngineRuntime runtime{kernel, fixture.lifecycle, fixture.mode, dross::RuntimeConfig{}};
  std::optional<dross::SaveBoundaryError> observed;
  auto subscription = kernel.events().subscribe_capability(
      [&](const dross::placement::EntityPlaced&, dross::EventReactionContext&) {
        const auto boundary = runtime.save_boundary();
        if (!boundary) {
          observed = boundary.error();
        }
      });
  REQUIRE(subscription);
  kernel.enqueue(scheduled_command(1, dross::Tick{0}, entity, 0));

  static_cast<void>(kernel.run_cycle());

  REQUIRE(observed);
  CHECK(*observed == dross::SaveBoundaryError::event_queue_draining);
}
