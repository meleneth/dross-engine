#pragma once

#include <dross/foundation/byte_codec.hpp>
#include <dross/foundation/result.hpp>
#include <dross/generated/grant_item.hpp>
#include <dross/generated/item_granted.hpp>
#include <dross/generated/item_removed.hpp>
#include <dross/generated/remove_item.hpp>
#include <dross/identity/entity_ref.hpp>

#include <cstdint>
#include <memory>
#include <vector>

namespace dross {

struct InventoryEntry {
  EntityId owner;
  ContentId item;
  std::uint32_t count;

  [[nodiscard]] auto operator<=>(const InventoryEntry&) const = default;
};

struct InventorySnapshot {
  std::vector<InventoryEntry> entries;

  [[nodiscard]] bool operator==(const InventorySnapshot&) const = default;
};

void encode_inventory_snapshot(ByteWriter& writer, const InventorySnapshot& snapshot);
[[nodiscard]] Result<InventorySnapshot, DecodeError> decode_inventory_snapshot(ByteReader& reader);

enum class InventoryRejection : std::uint8_t {
  wrong_world,
  unknown_owner,
  invalid_count,
  quantity_overflow,
  insufficient_quantity,
};

class InventoryRuntime {
public:
  class EventSink {
  public:
    virtual ~EventSink() = default;
    virtual void publish(const inventory::ItemGranted& event) = 0;
    virtual void publish(const inventory::ItemRemoved& event) = 0;
  };

  InventoryRuntime(WorldInstanceId world_instance, std::vector<EntityId> owners,
                   EventSink* events = nullptr);
  ~InventoryRuntime();
  InventoryRuntime(InventoryRuntime&&) noexcept;
  InventoryRuntime& operator=(InventoryRuntime&&) noexcept;
  InventoryRuntime(const InventoryRuntime&) = delete;
  InventoryRuntime& operator=(const InventoryRuntime&) = delete;

  [[nodiscard]] Result<void, InventoryRejection> handle(const inventory::GrantItem& command);
  [[nodiscard]] Result<void, InventoryRejection> handle(const inventory::RemoveItem& command);
  [[nodiscard]] std::uint32_t count(const EntityRef& owner, const ContentId& item) const;
  [[nodiscard]] bool has(const EntityRef& owner, const ContentId& item,
                         std::uint32_t count = 1) const;
  [[nodiscard]] InventorySnapshot snapshot() const;
  [[nodiscard]] bool restore(const InventorySnapshot& snapshot);

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace dross
