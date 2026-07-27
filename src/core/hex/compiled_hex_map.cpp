#include <dross/hex/compiled_hex_map.hpp>

#include <utility>

namespace dross {

std::optional<CellFacts> CompiledHexMap::cell(const HexCellId& cell_id) const {
  const auto found = cells_.find(cell_id);
  return found == cells_.end() ? std::nullopt : std::optional<CellFacts>{found->second};
}

std::optional<EdgeFacts> CompiledHexMap::edge(const HexCellId& first,
                                              const HexCellId& second) const {
  const auto key = EdgeKey::between(first, second);
  if (!key) {
    return std::nullopt;
  }
  const auto found = edges_.find(*key);
  return found == edges_.end() ? std::nullopt : std::optional<EdgeFacts>{found->second};
}

std::vector<HexCellId> CompiledHexMap::neighbors(const HexCellId& cell_id) const {
  std::vector<HexCellId> result;
  for (const auto& [key, facts] : edges_) {
    static_cast<void>(facts);
    if (key.first() == cell_id) {
      result.push_back(key.second());
    } else if (key.second() == cell_id) {
      result.push_back(key.first());
    }
  }
  return result;
}

std::vector<HexCellId> CompiledHexMap::cell_ids() const {
  std::vector<HexCellId> result;
  result.reserve(cells_.size());
  for (const auto& [id, facts] : cells_) {
    static_cast<void>(facts);
    result.push_back(id);
  }
  return result;
}

Result<void, MapBuildError> CompiledHexMapBuilder::add_cell(CellFacts facts) {
  if (cells_.contains(facts.id)) {
    return tl::unexpected{MapBuildError::duplicate_cell};
  }
  const auto cell_id = facts.id;
  cells_.emplace(cell_id, std::move(facts));
  return {};
}

Result<void, MapBuildError> CompiledHexMapBuilder::add_edge(const HexCellId& from,
                                                            const HexCellId& destination,
                                                            const DirectionalEdgeFacts from_to,
                                                            const DirectionalEdgeFacts to_from) {
  if (!cells_.contains(from) || !cells_.contains(destination)) {
    return tl::unexpected{MapBuildError::missing_cell};
  }
  auto key = EdgeKey::between(from, destination);
  if (!key) {
    return tl::unexpected{MapBuildError::identical_edge_endpoints};
  }
  if (edges_.contains(*key)) {
    return tl::unexpected{MapBuildError::duplicate_edge};
  }
  const auto from_is_first = from == key->first();
  edges_.emplace(
      *key, EdgeFacts{*key, from_is_first ? from_to : to_from, from_is_first ? to_from : from_to});
  return {};
}

Result<CompiledHexMap, MapBuildError> CompiledHexMapBuilder::build() && {
  return CompiledHexMap{std::move(cells_), std::move(edges_)};
}

} // namespace dross
