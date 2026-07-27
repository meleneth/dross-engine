#pragma once

#include <dross/foundation/result.hpp>
#include <dross/identity/ids.hpp>

#include <cstdint>
#include <set>

namespace dross {

struct EntityIdAllocatorSnapshot {
  std::uint64_t next_runtime_sequence;

  [[nodiscard]] constexpr bool operator==(const EntityIdAllocatorSnapshot&) const = default;
};

enum class EntityIdAllocationError : std::uint8_t {
  sequence_already_used,
  sequence_exhausted,
};

class EntityIdAllocator {
public:
  explicit EntityIdAllocator(std::uint64_t lineage,
                             EntityIdAllocatorSnapshot snapshot = EntityIdAllocatorSnapshot{1});

  [[nodiscard]] Result<EntityId, EntityIdAllocationError> allocate_runtime();
  [[nodiscard]] Result<EntityId, EntityIdAllocationError> reserve_authored(std::uint64_t sequence);
  [[nodiscard]] EntityIdAllocatorSnapshot snapshot() const noexcept;

private:
  std::uint64_t lineage_;
  std::uint64_t next_runtime_sequence_;
  std::set<std::uint64_t> used_sequences_;
};

} // namespace dross
