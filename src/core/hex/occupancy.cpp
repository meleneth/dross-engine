#include <dross/hex/occupancy.hpp>

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace dross {

Result<void, OccupancyError> OccupancyIndex::validate(const EntityId entity,
                                                      std::vector<HexCellId>& cells,
                                                      const bool entity_must_exist) const {
  const auto exists = by_entity_.contains(entity);
  if (entity_must_exist && !exists) {
    return tl::unexpected{
        OccupancyError{.reason = OccupancyErrorReason::entity_not_placed, .cell = std::nullopt}};
  }
  if (!entity_must_exist && exists) {
    return tl::unexpected{OccupancyError{.reason = OccupancyErrorReason::entity_already_placed,
                                         .cell = std::nullopt}};
  }
  std::ranges::sort(cells);
  const auto duplicate = std::ranges::adjacent_find(cells);
  if (duplicate != cells.end()) {
    return tl::unexpected{
        OccupancyError{.reason = OccupancyErrorReason::duplicate_cell, .cell = *duplicate}};
  }
  for (const auto& cell : cells) {
    const auto found = by_cell_.find(cell);
    if (found != by_cell_.end() && found->second != entity) {
      return tl::unexpected{OccupancyError{.reason = OccupancyErrorReason::occupied, .cell = cell}};
    }
  }
  return {};
}

Result<void, OccupancyError> OccupancyIndex::place(const EntityId entity,
                                                   std::vector<HexCellId> cells) {
  auto valid = validate(entity, cells, false);
  if (!valid) {
    return valid;
  }
  for (const auto& cell : cells) {
    by_cell_.emplace(cell, entity);
  }
  by_entity_.emplace(entity, std::move(cells));
  advance_revision();
  return {};
}

Result<void, OccupancyError> OccupancyIndex::move(const EntityId entity,
                                                  std::vector<HexCellId> cells) {
  auto valid = validate(entity, cells, true);
  if (!valid) {
    return valid;
  }
  for (const auto& old_cell : by_entity_.at(entity)) {
    by_cell_.erase(old_cell);
  }
  for (const auto& cell : cells) {
    by_cell_.emplace(cell, entity);
  }
  by_entity_.at(entity) = std::move(cells);
  advance_revision();
  return {};
}

Result<void, OccupancyError> OccupancyIndex::remove(const EntityId entity) {
  const auto found = by_entity_.find(entity);
  if (found == by_entity_.end()) {
    return tl::unexpected{
        OccupancyError{.reason = OccupancyErrorReason::entity_not_placed, .cell = std::nullopt}};
  }
  for (const auto& cell : found->second) {
    by_cell_.erase(cell);
  }
  by_entity_.erase(found);
  advance_revision();
  return {};
}

Result<void, OccupancyError>
OccupancyIndex::rebuild(const std::span<const OccupancyPlacement> placements) {
  OccupancyIndex rebuilt;
  auto ordered = std::vector<OccupancyPlacement>{placements.begin(), placements.end()};
  std::ranges::sort(ordered, {}, &OccupancyPlacement::entity);
  for (auto& placement : ordered) {
    const auto placed = rebuilt.place(placement.entity, std::move(placement.cells));
    if (!placed) {
      return placed;
    }
  }
  by_cell_ = std::move(rebuilt.by_cell_);
  by_entity_ = std::move(rebuilt.by_entity_);
  advance_revision();
  return {};
}

Result<void, OccupancyError>
OccupancyIndex::restore(const std::span<const OccupancyPlacement> placements,
                        const std::uint64_t revision) {
  if (!placements.empty() && revision == 0) {
    return tl::unexpected{
        OccupancyError{.reason = OccupancyErrorReason::invalid_revision, .cell = std::nullopt}};
  }
  OccupancyIndex restored;
  const auto rebuilt = restored.rebuild(placements);
  if (!rebuilt) {
    return rebuilt;
  }
  restored.revision_ = revision;
  by_cell_ = std::move(restored.by_cell_);
  by_entity_ = std::move(restored.by_entity_);
  revision_ = restored.revision_;
  return {};
}

std::optional<EntityId> OccupancyIndex::occupant(const HexCellId& cell) const {
  const auto found = by_cell_.find(cell);
  return found == by_cell_.end() ? std::nullopt : std::optional<EntityId>{found->second};
}

bool OccupancyIndex::can_occupy(const std::span<const HexCellId> cells,
                                const std::optional<EntityId> ignored) const {
  return std::ranges::all_of(cells, [this, ignored](const HexCellId& cell) {
    const auto found = by_cell_.find(cell);
    return found == by_cell_.end() || (ignored && found->second == *ignored);
  });
}

std::vector<OccupancyEntry> OccupancyIndex::entries() const {
  std::vector<OccupancyEntry> result;
  result.reserve(by_cell_.size());
  for (const auto& [cell, entity] : by_cell_) {
    result.push_back(OccupancyEntry{.cell = cell, .entity = entity});
  }
  return result;
}

void OccupancyIndex::advance_revision() {
  if (revision_ == std::numeric_limits<std::uint64_t>::max()) {
    throw std::overflow_error{"occupancy revision exhausted"};
  }
  ++revision_;
}

} // namespace dross
