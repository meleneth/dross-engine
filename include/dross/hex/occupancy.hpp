#pragma once

#include <dross/foundation/result.hpp>
#include <dross/hex/hex_topology.hpp>
#include <dross/identity/ids.hpp>

#include <compare>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <vector>

namespace dross {

enum class OccupancyErrorReason : std::uint8_t {
  occupied,
  duplicate_cell,
  entity_already_placed,
  entity_not_placed,
  invalid_revision,
};

struct OccupancyError {
  OccupancyErrorReason reason;
  std::optional<HexCellId> cell;
};

struct OccupancyPlacement {
  EntityId entity;
  std::vector<HexCellId> cells;
};

struct OccupancyEntry {
  HexCellId cell;
  EntityId entity;

  [[nodiscard]] auto operator<=>(const OccupancyEntry&) const = default;
};

class OccupancyIndex {
public:
  [[nodiscard]] Result<void, OccupancyError> place(EntityId entity, std::vector<HexCellId> cells);
  [[nodiscard]] Result<void, OccupancyError> move(EntityId entity, std::vector<HexCellId> cells);
  [[nodiscard]] Result<void, OccupancyError> remove(EntityId entity);
  [[nodiscard]] Result<void, OccupancyError>
  rebuild(std::span<const OccupancyPlacement> placements);
  [[nodiscard]] Result<void, OccupancyError> restore(std::span<const OccupancyPlacement> placements,
                                                     std::uint64_t revision);

  [[nodiscard]] std::optional<EntityId> occupant(const HexCellId& cell) const;
  [[nodiscard]] bool can_occupy(std::span<const HexCellId> cells,
                                std::optional<EntityId> ignored = std::nullopt) const;
  [[nodiscard]] std::vector<OccupancyEntry> entries() const;
  [[nodiscard]] std::uint64_t revision() const noexcept { return revision_; }
  [[nodiscard]] bool empty() const noexcept { return by_cell_.empty(); }

private:
  void advance_revision();
  [[nodiscard]] Result<void, OccupancyError>
  validate(EntityId entity, std::vector<HexCellId>& cells, bool entity_must_exist) const;

  std::map<HexCellId, EntityId> by_cell_;
  std::map<EntityId, std::vector<HexCellId>> by_entity_;
  std::uint64_t revision_{0};
};

} // namespace dross
