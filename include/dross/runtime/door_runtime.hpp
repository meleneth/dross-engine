#pragma once

#include <dross/foundation/byte_codec.hpp>
#include <dross/foundation/result.hpp>
#include <dross/generated/door_closed.hpp>
#include <dross/generated/door_opened.hpp>
#include <dross/hex/traversal.hpp>
#include <dross/identity/entity_ref.hpp>

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

struct DoorSnapshot {
  DoorState state;

  [[nodiscard]] auto operator<=>(const DoorSnapshot&) const = default;
};

void encode_door_snapshot(ByteWriter& writer, DoorSnapshot snapshot);
[[nodiscard]] Result<DoorSnapshot, DecodeError> decode_door_snapshot(ByteReader& reader);

class DoorRuntime final : public EdgeTraversalPolicy {
public:
  class EventSink {
  public:
    virtual ~EventSink() = default;
    virtual void publish(const door::DoorOpened& event) = 0;
    virtual void publish(const door::DoorClosed& event) = 0;
  };

  DoorRuntime(EntityRef entity, EdgeFootprint footprint, DoorState initial_state,
              EventSink* events = nullptr);
  ~DoorRuntime();
  DoorRuntime(DoorRuntime&&) noexcept;
  DoorRuntime& operator=(DoorRuntime&&) noexcept;
  DoorRuntime(const DoorRuntime&) = delete;
  DoorRuntime& operator=(const DoorRuntime&) = delete;

  [[nodiscard]] bool open();
  [[nodiscard]] bool close();
  [[nodiscard]] DoorState state() const;
  [[nodiscard]] bool allows(const EdgeKey& edge) const override;
  [[nodiscard]] DoorSnapshot snapshot() const;
  [[nodiscard]] bool restore(DoorSnapshot snapshot);
  void acknowledge_presentation(std::uint64_t acknowledgement_id);

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace dross
