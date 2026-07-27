#pragma once

#include <dross/foundation/quantities.hpp>
#include <dross/foundation/result.hpp>
#include <dross/hex/hex_topology.hpp>
#include <dross/identity/content_id.hpp>

#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace dross {

enum class Clearance : std::uint8_t {
  open,
  low,
  blocked,
};

struct CellFacts {
  HexCellId id;
  Millimeters surface_height;
  ContentId terrain;
  MovementCost base_cost;
  Clearance clearance;
  bool traversable;
  std::vector<ContentId> semantic_tags;
};

struct DirectionalEdgeFacts {
  bool traversable;
  MovementCost cost;

  [[nodiscard]] bool operator==(const DirectionalEdgeFacts&) const = default;
};

class EdgeFacts {
public:
  [[nodiscard]] const EdgeKey& key() const noexcept { return key_; }
  [[nodiscard]] const DirectionalEdgeFacts& from_to(const HexCellId& from) const noexcept {
    return from == key_.first() ? first_to_second_ : second_to_first_;
  }

private:
  friend class CompiledHexMapBuilder;
  EdgeFacts(EdgeKey key, DirectionalEdgeFacts first_to_second, DirectionalEdgeFacts second_to_first)
      : key_{std::move(key)}, first_to_second_{first_to_second}, second_to_first_{second_to_first} {
  }

  EdgeKey key_;
  DirectionalEdgeFacts first_to_second_;
  DirectionalEdgeFacts second_to_first_;
};

enum class MapBuildError : std::uint8_t {
  duplicate_cell,
  duplicate_edge,
  missing_cell,
  identical_edge_endpoints,
};

class CompiledHexMap {
public:
  [[nodiscard]] std::optional<CellFacts> cell(const HexCellId& id) const;
  [[nodiscard]] std::optional<EdgeFacts> edge(const HexCellId& first,
                                              const HexCellId& second) const;
  [[nodiscard]] std::vector<HexCellId> neighbors(const HexCellId& id) const;
  [[nodiscard]] std::vector<HexCellId> cell_ids() const;

private:
  friend class CompiledHexMapBuilder;
  CompiledHexMap(std::map<HexCellId, CellFacts> cells, std::map<EdgeKey, EdgeFacts> edges)
      : cells_{std::move(cells)}, edges_{std::move(edges)} {}

  std::map<HexCellId, CellFacts> cells_;
  std::map<EdgeKey, EdgeFacts> edges_;
};

class CompiledHexMapBuilder {
public:
  [[nodiscard]] Result<void, MapBuildError> add_cell(CellFacts facts);
  [[nodiscard]] Result<void, MapBuildError> add_edge(const HexCellId& from,
                                                     const HexCellId& destination,
                                                     DirectionalEdgeFacts from_to,
                                                     DirectionalEdgeFacts to_from);
  [[nodiscard]] Result<CompiledHexMap, MapBuildError> build() &&;

private:
  std::map<HexCellId, CellFacts> cells_;
  std::map<EdgeKey, EdgeFacts> edges_;
};

} // namespace dross
