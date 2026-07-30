#include <dross/hex/path_planner.hpp>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

constexpr std::int32_t map_cell_count = 256;
constexpr std::size_t sample_count = 100;
constexpr std::size_t plans_per_sample = 10;
constexpr std::size_t p95_numerator = 95;
constexpr std::size_t percentile_denominator = 100;

[[nodiscard]] dross::ContentId content_id(const char* value) {
  return dross::ContentId::parse(value).value();
}

[[nodiscard]] dross::HexCellId cell(const std::int32_t column) {
  return dross::HexCellId{
      .region = dross::RegionId{content_id("benchmark:large_path")},
      .coord = dross::HexCoord{.q = column, .r = 0},
      .layer = 0,
  };
}

[[nodiscard]] dross::CompiledHexMap make_map() {
  dross::CompiledHexMapBuilder builder;
  for (std::int32_t column = 0; column < map_cell_count; ++column) {
    const auto added = builder.add_cell(dross::CellFacts{
        .id = cell(column),
        .surface_height = dross::Millimeters{0},
        .terrain = content_id("benchmark:floor"),
        .base_cost = dross::MovementCost{1},
        .clearance = dross::Clearance::open,
        .traversable = true,
        .semantic_tags = {},
    });
    if (!added) {
      throw std::logic_error{"benchmark map cell construction failed"};
    }
  }
  const dross::DirectionalEdgeFacts open{
      .traversable = true,
      .cost = dross::MovementCost{1},
  };
  for (std::int32_t column = 1; column < map_cell_count; ++column) {
    if (!builder.add_edge(cell(column - 1), cell(column), open, open)) {
      throw std::logic_error{"benchmark map edge construction failed"};
    }
  }
  return std::move(builder).build().value();
}

[[nodiscard]] double percentile(std::vector<double> values, const std::size_t numerator,
                                const std::size_t denominator) {
  std::ranges::sort(values);
  const auto rank = (values.size() * numerator + denominator - 1) / denominator;
  return values[rank - 1];
}

int run_benchmark() {
  const auto map = make_map();
  const auto footprint =
      dross::FootprintDefinition::create(dross::FootprintId{content_id("benchmark:single")},
                                         {dross::HexCoord{.q = 0, .r = 0}})
          .value();
  const dross::WeightedAStarPathPlanner planner;
  const dross::OccupancyIndex occupancy;
  const dross::EntityId actor{1, 1};
  const dross::HexPose start{.anchor = cell(0), .facing = dross::HexFacing::east};
  const dross::HexPose goal{.anchor = cell(map_cell_count - 1), .facing = dross::HexFacing::east};
  const dross::TraversalPolicy policy{.rotation_cost = dross::MovementCost{1}};

  const auto warmup = planner.plan(map, occupancy, footprint, start, goal, policy, actor);
  if (!warmup) {
    std::cerr << "large path benchmark warmup failed\n";
    return 1;
  }

  std::vector<double> samples;
  samples.reserve(sample_count);
  for (std::size_t sample = 0; sample < sample_count; ++sample) {
    const auto started = std::chrono::steady_clock::now();
    for (std::size_t iteration = 0; iteration < plans_per_sample; ++iteration) {
      const auto path = planner.plan(map, occupancy, footprint, start, goal, policy, actor);
      if (!path || path->poses.size() != warmup->poses.size()) {
        std::cerr << "large path benchmark produced an inconsistent result\n";
        return 1;
      }
    }
    const auto elapsed =
        std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - started);
    samples.push_back(elapsed.count() / static_cast<double>(plans_per_sample));
  }

  std::cout << std::fixed << std::setprecision(3) << "path_preview_large cells=" << map_cell_count
            << " poses=" << warmup->poses.size() << " median_us=" << percentile(samples, 1, 2)
            << " p95_us=" << percentile(samples, p95_numerator, percentile_denominator)
            << " samples=" << samples.size() << '\n';
  return 0;
}

} // namespace

int main() {
  try {
    return run_benchmark();
  } catch (const std::exception& error) {
    std::cerr << "benchmark failed: " << error.what() << '\n';
  } catch (...) {
    std::cerr << "benchmark failed with an unknown exception\n";
  }
  return 1;
}
