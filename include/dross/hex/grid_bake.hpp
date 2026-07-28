#pragma once

#include <dross/foundation/result.hpp>
#include <dross/hex/compiled_hex_map.hpp>

#include <compare>
#include <cstdint>
#include <map>
#include <optional>
#include <vector>

namespace dross {

struct GridIdentity {
  RegionId region;
  std::int64_t origin_x_mm;
  std::int64_t origin_y_mm;
  std::int64_t origin_z_mm;
  std::int64_t radius_mm;

  [[nodiscard]] auto operator<=>(const GridIdentity&) const = default;
};

struct HexBakeProfile {
  std::uint32_t algorithm_version;
  std::int64_t quantization_mm;
  std::int64_t maximum_height_variance_mm;
  std::uint32_t required_sample_count;
  ContentId terrain;
  MovementCost movement_cost;
};

struct CellBakeEvidence {
  HexCellId id;
  std::vector<std::int64_t> surface_samples_mm;
  bool standing_clearance;
  ContentId source;
};

struct EdgeBakeEvidence {
  HexCellId from;
  HexCellId to;
  bool from_to_clear;
  bool to_from_clear;
};

struct GridBake {
  GridIdentity identity;
  std::uint32_t profile_version;
  std::vector<CellBakeEvidence> cells;
  std::vector<EdgeBakeEvidence> edges;
};

enum class CellTraversabilityOverride : std::uint8_t {
  automatic,
  force_traversable,
  force_blocked,
};

struct GridOverrides {
  GridIdentity identity;
  std::vector<std::pair<HexCellId, CellTraversabilityOverride>> cells;
};

enum class BakeReason : std::uint8_t {
  automatic_traversable,
  insufficient_samples,
  height_variance,
  insufficient_clearance,
};

struct ClassifiedBakeCell {
  CellFacts facts;
  Millimeters surface_height;
  Clearance clearance;
  bool traversable;
  BakeReason reason;
};

enum class BakeClassifyError : std::uint8_t {
  invalid_profile,
  insufficient_samples,
};

[[nodiscard]] Result<ClassifiedBakeCell, BakeClassifyError>
classify_bake_cell(const CellBakeEvidence& evidence, const HexBakeProfile& profile);

enum class CellProvenance : std::uint8_t {
  automatic,
  manual_force_traversable,
  manual_force_blocked,
};

enum class GridCompileErrorReason : std::uint8_t {
  invalid_profile,
  identity_mismatch,
  profile_version_mismatch,
  duplicate_evidence,
  duplicate_override,
  orphan_override,
  invalid_edge,
};

struct GridCompileError {
  GridCompileErrorReason reason;
  std::optional<HexCellId> cell;
};

struct CompiledGridBake {
  CompiledHexMap map;
  std::map<HexCellId, CellProvenance> provenance;
  std::map<HexCellId, BakeReason> reasons;
};

[[nodiscard]] Result<CompiledGridBake, GridCompileError>
compile_grid_bake(const GridBake& bake, const GridOverrides& overrides,
                  const HexBakeProfile& profile);

} // namespace dross
