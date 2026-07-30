#include <dross/runtime/inventory_runtime.hpp>

#include <algorithm>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <utility>

namespace dross {
namespace {

using InventoryKey = std::pair<EntityId, ContentId>;

DecodeError decode_error(const ByteReader& reader) {
  return DecodeError{.position = reader.remaining(), .reason = DecodeErrorReason::invalid_length};
}

} // namespace

void encode_inventory_snapshot(ByteWriter& writer, const InventorySnapshot& snapshot) {
  writer.write_u32(static_cast<std::uint32_t>(snapshot.entries.size()));
  for (const auto& entry : snapshot.entries) {
    writer.write(entry.owner);
    writer.write(entry.item);
    writer.write_u32(entry.count);
  }
}

Result<InventorySnapshot, DecodeError> decode_inventory_snapshot(ByteReader& reader) {
  const auto size = reader.read_u32();
  if (!size) {
    return tl::unexpected{size.error()};
  }
  InventorySnapshot snapshot;
  snapshot.entries.reserve(*size);
  for (std::uint32_t index = 0; index < *size; ++index) {
    auto owner = reader.read_entity_id();
    auto item = reader.read_content_id();
    auto count = reader.read_u32();
    if (!owner) {
      return tl::unexpected{owner.error()};
    }
    if (!item) {
      return tl::unexpected{item.error()};
    }
    if (!count) {
      return tl::unexpected{count.error()};
    }
    if (*count == 0) {
      return tl::unexpected{decode_error(reader)};
    }
    snapshot.entries.push_back(
        InventoryEntry{.owner = *owner, .item = *std::move(item), .count = *count});
  }
  return snapshot;
}

struct InventoryRuntime::Impl {
  WorldInstanceId world_instance;
  std::set<EntityId> owners;
  EventSink* events;
  std::map<InventoryKey, std::uint32_t> counts;

  [[nodiscard]] std::optional<InventoryRejection> validate(const EntityRef& owner,
                                                           const std::uint32_t amount) const {
    if (owner.world_instance() != world_instance) {
      return InventoryRejection::wrong_world;
    }
    if (!owners.contains(owner.id())) {
      return InventoryRejection::unknown_owner;
    }
    if (amount == 0) {
      return InventoryRejection::invalid_count;
    }
    return {};
  }
};

InventoryRuntime::InventoryRuntime(const WorldInstanceId world_instance,
                                   std::vector<EntityId> owners, EventSink* events)
    : impl_{std::make_unique<Impl>(Impl{
          .world_instance = world_instance,
          .owners = std::set<EntityId>{owners.begin(), owners.end()},
          .events = events,
          .counts = {},
      })} {}

InventoryRuntime::~InventoryRuntime() = default;
InventoryRuntime::InventoryRuntime(InventoryRuntime&&) noexcept = default;
InventoryRuntime& InventoryRuntime::operator=(InventoryRuntime&&) noexcept = default;

Result<void, InventoryRejection> InventoryRuntime::handle(const inventory::GrantItem& command) {
  if (const auto invalid = impl_->validate(command.owner, command.count)) {
    return tl::unexpected{*invalid};
  }
  const auto key = InventoryKey{command.owner.id(), command.item};
  const auto current = impl_->counts.contains(key) ? impl_->counts.at(key) : 0U;
  if (command.count > std::numeric_limits<std::uint32_t>::max() - current) {
    return tl::unexpected{InventoryRejection::quantity_overflow};
  }
  const auto resulting = current + command.count;
  impl_->counts.insert_or_assign(key, resulting);
  if (impl_->events != nullptr) {
    impl_->events->publish(inventory::ItemGranted{
        .owner = command.owner,
        .item = command.item,
        .count = command.count,
        .new_count = resulting,
    });
  }
  return {};
}

Result<void, InventoryRejection> InventoryRuntime::handle(const inventory::RemoveItem& command) {
  if (const auto invalid = impl_->validate(command.owner, command.count)) {
    return tl::unexpected{*invalid};
  }
  const auto key = InventoryKey{command.owner.id(), command.item};
  const auto found = impl_->counts.find(key);
  if (found == impl_->counts.end() || found->second < command.count) {
    return tl::unexpected{InventoryRejection::insufficient_quantity};
  }
  const auto resulting = found->second - command.count;
  if (resulting == 0) {
    impl_->counts.erase(found);
  } else {
    found->second = resulting;
  }
  if (impl_->events != nullptr) {
    impl_->events->publish(inventory::ItemRemoved{
        .owner = command.owner,
        .item = command.item,
        .count = command.count,
        .new_count = resulting,
    });
  }
  return {};
}

std::uint32_t InventoryRuntime::count(const EntityRef& owner, const ContentId& item) const {
  if (owner.world_instance() != impl_->world_instance || !impl_->owners.contains(owner.id())) {
    return 0;
  }
  const auto found = impl_->counts.find(InventoryKey{owner.id(), item});
  return found == impl_->counts.end() ? 0U : found->second;
}

bool InventoryRuntime::has(const EntityRef& owner, const ContentId& item,
                           const std::uint32_t required) const {
  return required != 0 && count(owner, item) >= required;
}

InventorySnapshot InventoryRuntime::snapshot() const {
  InventorySnapshot snapshot;
  snapshot.entries.reserve(impl_->counts.size());
  for (const auto& [key, count] : impl_->counts) {
    snapshot.entries.push_back(
        InventoryEntry{.owner = key.first, .item = key.second, .count = count});
  }
  return snapshot;
}

bool InventoryRuntime::restore(const InventorySnapshot& snapshot) {
  std::map<InventoryKey, std::uint32_t> restored;
  for (const auto& entry : snapshot.entries) {
    if (!impl_->owners.contains(entry.owner) || entry.count == 0 ||
        !restored.emplace(InventoryKey{entry.owner, entry.item}, entry.count).second) {
      return false;
    }
  }
  impl_->counts = std::move(restored);
  return true;
}

} // namespace dross
