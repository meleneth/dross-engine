#include <dross/hex/grid_bake.hpp>

#include <dross/hex/hex_coord.hpp>

#include <algorithm>
#include <cstdlib>
#include <map>
#include <numeric>
#include <utility>

namespace dross {
namespace {

bool valid_profile(const HexBakeProfile& profile) {
  return profile.algorithm_version > 0 && profile.quantization_mm > 0 &&
         profile.maximum_height_variance_mm >= 0 && profile.required_sample_count >= 4;
}

struct QuantizationQuantum {
  std::int64_t value;
};

std::int64_t quantize(const std::int64_t value, const QuantizationQuantum quantum) {
  const auto magnitude = std::abs(value);
  const auto rounded = ((magnitude + quantum.value / 2) / quantum.value) * quantum.value;
  return value < 0 ? -rounded : rounded;
}

BakeReason classify_reason(const bool uneven, const bool standing_clearance) {
  if (uneven) {
    return BakeReason::height_variance;
  }
  if (!standing_clearance) {
    return BakeReason::insufficient_clearance;
  }
  return BakeReason::automatic_traversable;
}

Result<std::map<HexCellId, CellTraversabilityOverride>, GridCompileError>
canonicalize_overrides(const GridOverrides& overrides,
                       const std::map<HexCellId, ClassifiedBakeCell>& classified) {
  std::map<HexCellId, CellTraversabilityOverride> canonical;
  for (const auto& [cell, override_value] : overrides.cells) {
    if (!classified.contains(cell)) {
      return tl::unexpected{
          GridCompileError{.reason = GridCompileErrorReason::orphan_override, .cell = cell}};
    }
    if (!canonical.emplace(cell, override_value).second) {
      return tl::unexpected{
          GridCompileError{.reason = GridCompileErrorReason::duplicate_override, .cell = cell}};
    }
  }
  return canonical;
}

} // namespace

Result<ClassifiedBakeCell, BakeClassifyError> classify_bake_cell(const CellBakeEvidence& evidence,
                                                                 const HexBakeProfile& profile) {
  if (!valid_profile(profile)) {
    return tl::unexpected{BakeClassifyError::invalid_profile};
  }
  if (evidence.surface_samples_mm.size() < profile.required_sample_count) {
    return tl::unexpected{BakeClassifyError::insufficient_samples};
  }
  const auto [minimum, maximum] = std::ranges::minmax_element(evidence.surface_samples_mm);
  const auto sum = std::accumulate(evidence.surface_samples_mm.begin(),
                                   evidence.surface_samples_mm.end(), std::int64_t{0});
  const auto mean = sum / static_cast<std::int64_t>(evidence.surface_samples_mm.size());
  const auto height =
      Millimeters{quantize(mean, QuantizationQuantum{.value = profile.quantization_mm})};
  const auto uneven = *maximum - *minimum > profile.maximum_height_variance_mm;
  const auto reason = classify_reason(uneven, evidence.standing_clearance);
  const auto clearance = evidence.standing_clearance ? Clearance::open : Clearance::blocked;
  const auto traversable = !uneven && evidence.standing_clearance;
  return ClassifiedBakeCell{
      .facts =
          CellFacts{
              .id = evidence.id,
              .surface_height = height,
              .terrain = profile.terrain,
              .base_cost = profile.movement_cost,
              .clearance = clearance,
              .traversable = traversable,
              .semantic_tags = {},
          },
      .surface_height = height,
      .clearance = clearance,
      .traversable = traversable,
      .reason = reason,
  };
}

Result<CompiledGridBake, GridCompileError> compile_grid_bake(const GridBake& bake,
                                                             const GridOverrides& overrides,
                                                             const HexBakeProfile& profile) {
  if (!valid_profile(profile)) {
    return tl::unexpected{
        GridCompileError{.reason = GridCompileErrorReason::invalid_profile, .cell = {}}};
  }
  if (bake.identity != overrides.identity) {
    return tl::unexpected{
        GridCompileError{.reason = GridCompileErrorReason::identity_mismatch, .cell = {}}};
  }
  if (bake.profile_version != profile.algorithm_version) {
    return tl::unexpected{
        GridCompileError{.reason = GridCompileErrorReason::profile_version_mismatch, .cell = {}}};
  }

  std::map<HexCellId, ClassifiedBakeCell> classified;
  for (const auto& evidence : bake.cells) {
    if (evidence.id.region != bake.identity.region || classified.contains(evidence.id)) {
      return tl::unexpected{GridCompileError{.reason = GridCompileErrorReason::duplicate_evidence,
                                             .cell = evidence.id}};
    }
    auto cell = classify_bake_cell(evidence, profile);
    if (!cell) {
      return tl::unexpected{
          GridCompileError{.reason = GridCompileErrorReason::invalid_profile, .cell = evidence.id}};
    }
    classified.emplace(evidence.id, std::move(*cell));
  }

  auto canonical_overrides = canonicalize_overrides(overrides, classified);
  if (!canonical_overrides) {
    return tl::unexpected{canonical_overrides.error()};
  }

  CompiledHexMapBuilder builder;
  std::map<HexCellId, CellProvenance> provenance;
  std::map<HexCellId, BakeReason> reasons;
  for (auto& [cell_id, cell] : classified) {
    const auto found = canonical_overrides->find(cell_id);
    const auto override_value =
        found == canonical_overrides->end() ? CellTraversabilityOverride::automatic : found->second;
    switch (override_value) {
    case CellTraversabilityOverride::automatic:
      provenance.emplace(cell_id, CellProvenance::automatic);
      break;
    case CellTraversabilityOverride::force_traversable:
      cell.facts.traversable = true;
      cell.facts.clearance = Clearance::open;
      provenance.emplace(cell_id, CellProvenance::manual_force_traversable);
      break;
    case CellTraversabilityOverride::force_blocked:
      cell.facts.traversable = false;
      provenance.emplace(cell_id, CellProvenance::manual_force_blocked);
      break;
    }
    reasons.emplace(cell_id, cell.reason);
    if (!builder.add_cell(std::move(cell.facts))) {
      return tl::unexpected{
          GridCompileError{.reason = GridCompileErrorReason::duplicate_evidence, .cell = cell_id}};
    }
  }

  auto edges = bake.edges;
  std::ranges::sort(edges, [](const EdgeBakeEvidence& left, const EdgeBakeEvidence& right) {
    return std::pair{left.from, left.to} < std::pair{right.from, right.to};
  });
  for (const auto& edge : edges) {
    if (edge.from.region != edge.to.region || edge.from.layer != edge.to.layer ||
        hex_distance(edge.from.coord, edge.to.coord) != 1 ||
        !builder.add_edge(
            edge.from, edge.to,
            DirectionalEdgeFacts{.traversable = edge.from_to_clear, .cost = profile.movement_cost},
            DirectionalEdgeFacts{.traversable = edge.to_from_clear,
                                 .cost = profile.movement_cost})) {
      return tl::unexpected{
          GridCompileError{.reason = GridCompileErrorReason::invalid_edge, .cell = edge.from}};
    }
  }
  auto map = std::move(builder).build();
  if (!map) {
    return tl::unexpected{
        GridCompileError{.reason = GridCompileErrorReason::invalid_edge, .cell = {}}};
  }
  return CompiledGridBake{
      .map = std::move(*map), .provenance = std::move(provenance), .reasons = std::move(reasons)};
}

} // namespace dross
