#pragma once

#include <dross/foundation/result.hpp>
#include <dross/hex/hex_topology.hpp>
#include <dross/identity/content_id.hpp>

#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace dross {

class FootprintId {
public:
  explicit FootprintId(ContentId value) : value_{std::move(value)} {}

  [[nodiscard]] const ContentId& content_id() const noexcept { return value_; }
  [[nodiscard]] auto operator<=>(const FootprintId&) const = default;

private:
  ContentId value_;
};

enum class FootprintError : std::uint8_t {
  missing_origin,
  duplicate_offset,
};

class FootprintDefinition {
public:
  [[nodiscard]] static Result<FootprintDefinition, FootprintError>
  create(FootprintId id, std::vector<HexCoord> offsets);

  [[nodiscard]] const FootprintId& id() const noexcept { return id_; }
  [[nodiscard]] std::span<const HexCoord> offsets() const noexcept { return offsets_; }
  [[nodiscard]] std::vector<HexCoord> rotated(HexFacing facing) const;
  [[nodiscard]] std::vector<HexCellId> expand(const HexPose& pose) const;

private:
  FootprintDefinition(FootprintId id, std::vector<HexCoord> offsets)
      : id_{std::move(id)}, offsets_{std::move(offsets)} {}

  FootprintId id_;
  std::vector<HexCoord> offsets_;
};

} // namespace dross
