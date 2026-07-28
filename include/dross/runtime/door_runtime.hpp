#pragma once

#include <dross/foundation/result.hpp>
#include <dross/hex/hex_topology.hpp>

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace dross {

enum class EdgeFootprintError : std::uint8_t {
  empty,
  duplicate_edge,
  non_adjacent_edge,
};

class EdgeFootprint {
public:
  [[nodiscard]] static Result<EdgeFootprint, EdgeFootprintError> create(std::vector<EdgeKey> edges);

  [[nodiscard]] const std::vector<EdgeKey>& edges() const noexcept { return edges_; }
  [[nodiscard]] bool contains(const EdgeKey& edge) const;

private:
  explicit EdgeFootprint(std::vector<EdgeKey> edges) : edges_{std::move(edges)} {}
  std::vector<EdgeKey> edges_;
};

enum class DoorState : std::uint8_t {
  closed,
  open,
};

class DoorRuntime {
public:
  DoorRuntime(EdgeFootprint footprint, DoorState initial_state);
  ~DoorRuntime();
  DoorRuntime(DoorRuntime&&) noexcept;
  DoorRuntime& operator=(DoorRuntime&&) noexcept;
  DoorRuntime(const DoorRuntime&) = delete;
  DoorRuntime& operator=(const DoorRuntime&) = delete;

  [[nodiscard]] bool open();
  [[nodiscard]] bool close();
  [[nodiscard]] DoorState state() const;
  [[nodiscard]] bool allows(const EdgeKey& edge) const;
  void acknowledge_presentation(std::uint64_t acknowledgement_id);

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace dross
