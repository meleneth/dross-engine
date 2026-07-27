#include "command_event_kernel_scenario.hpp"

#include <dross/runtime/command_event_kernel.hpp>

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <utility>

namespace {

constexpr int scenario_error = 5;
constexpr std::uint64_t scenario_lineage = 31;
constexpr std::uint64_t scenario_instance = 8;
constexpr std::uint64_t scenario_correlation = 77;
constexpr std::uint64_t initial_causation = 101;
constexpr std::uint64_t rejected_causation = 103;

dross::ContentId content_id(const char* value) { return dross::ContentId::parse(value).value(); }

dross::HexCellId cell(const int column) {
  return dross::HexCellId{
      .region = dross::RegionId{content_id("demo:arena")},
      .coord = dross::HexCoord{.q = column, .r = 0},
      .layer = 0,
  };
}

dross::CompiledHexMap make_map() {
  dross::CompiledHexMapBuilder builder;
  for (int column = 0; column < 3; ++column) {
    const auto added = builder.add_cell(dross::CellFacts{
        .id = cell(column),
        .surface_height = dross::Millimeters{0},
        .terrain = content_id("demo:floor"),
        .base_cost = dross::MovementCost{1},
        .clearance = dross::Clearance::open,
        .traversable = true,
        .semantic_tags = {},
    });
    if (!added) {
      throw std::logic_error{"command-event scenario map construction failed"};
    }
  }
  return std::move(builder).build().value();
}

dross::PlaceEntityEnvelope command(const std::uint64_t command_id, const dross::EntityRef entity,
                                   const int column, const std::uint64_t causation) {
  return dross::PlaceEntityEnvelope{
      .metadata =
          dross::CommandMetadata{
              .id = dross::CommandId{command_id},
              .tick = dross::Tick{1},
              .source = dross::CommandSource::headless_test,
              .causation = dross::CausationId{causation},
              .correlation = dross::CorrelationId{scenario_correlation},
          },
      .payload =
          dross::placement::PlaceEntity{
              .entity = entity,
              .target =
                  dross::HexPose{
                      .anchor = cell(column),
                      .facing = dross::HexFacing::east,
                  },
          },
  };
}

} // namespace

int run_command_event_kernel_scenario() {
  dross::WorldStorage world{dross::WorldConfig{
      .lineage = scenario_lineage, .instance_id = dross::WorldInstanceId{scenario_instance}}};
  const auto first = world.write().spawn(dross::SpawnPlan::runtime());
  const auto second = world.write().spawn(dross::SpawnPlan::runtime());
  const auto blocked = world.write().spawn(dross::SpawnPlan::runtime());
  if (!first || !second || !blocked) {
    std::cerr << "command-event entity setup failed\n";
    return scenario_error;
  }

  dross::HeadlessPlacementScriptPort scripts;
  scripts.reject_cell(cell(2), content_id("demo:warded"));
  dross::InMemoryTraceSink trace;
  dross::CommandEventKernel kernel{world, make_map(), scripts, trace};
  const auto first_command = command(1, *first, 0, initial_causation);
  auto follow_up = kernel.events().subscribe_capability(
      [&first, &second](const dross::placement::EntityPlaced& event,
                        dross::EventReactionContext& context) {
        if (event.entity == *first) {
          context.enqueue_follow_up(command(2, *second, 1, initial_causation));
        }
      });
  if (!follow_up) {
    std::cerr << "command-event listener setup failed\n";
    return scenario_error;
  }

  kernel.enqueue(first_command);
  const auto first_cycle = kernel.run_cycle();
  const auto follow_up_cycle = kernel.run_cycle();
  kernel.enqueue(command(3, *blocked, 2, rejected_causation));
  const auto rejected_cycle = kernel.run_cycle();
  const auto revision = kernel.occupancy().revision();
  kernel.enqueue(first_command);
  const auto duplicate_cycle = kernel.run_cycle();

  const auto inspection = kernel.last_placement();
  if (first_cycle.size() != 1 || follow_up_cycle.size() != 1 || rejected_cycle.size() != 1 ||
      duplicate_cycle.size() != 1 || !first_cycle.front().accepted ||
      !follow_up_cycle.front().accepted ||
      rejected_cycle.front().rejection != dross::CommandRejection::script_rejected ||
      duplicate_cycle.front() != first_cycle.front() || kernel.occupancy().revision() != revision ||
      !inspection || inspection->entity != *second || trace.events().size() != 2 ||
      trace.commands().size() != 4) {
    std::cerr << "command-event kernel scenario failed\n";
    return scenario_error;
  }

  std::cout << "command-event-kernel accepted=2 rejected=script_rejected"
               " duplicate=previous_result follow_up=later"
            << " occupancy=" << kernel.occupancy().entries().size()
            << " inspected=" << inspection->entity.id()
            << " command_traces=" << trace.commands().size()
            << " event_traces=" << trace.events().size() << '\n';
  return 0;
}
