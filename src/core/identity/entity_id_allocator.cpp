#include <dross/identity/entity_id_allocator.hpp>

#include <limits>

namespace dross {

EntityIdAllocator::EntityIdAllocator(const std::uint64_t lineage,
                                     const EntityIdAllocatorSnapshot snapshot)
    : lineage_{lineage}, next_runtime_sequence_{snapshot.next_runtime_sequence} {}

Result<EntityId, EntityIdAllocationError> EntityIdAllocator::allocate_runtime() {
  while (used_sequences_.contains(next_runtime_sequence_)) {
    if (next_runtime_sequence_ == std::numeric_limits<std::uint64_t>::max()) {
      return tl::unexpected{EntityIdAllocationError::sequence_exhausted};
    }
    ++next_runtime_sequence_;
  }

  const auto allocated = next_runtime_sequence_;
  used_sequences_.insert(allocated);
  if (next_runtime_sequence_ != std::numeric_limits<std::uint64_t>::max()) {
    ++next_runtime_sequence_;
  }
  return EntityId{lineage_, allocated};
}

Result<EntityId, EntityIdAllocationError>
EntityIdAllocator::reserve_authored(const std::uint64_t sequence) {
  if (used_sequences_.contains(sequence)) {
    return tl::unexpected{EntityIdAllocationError::sequence_already_used};
  }
  used_sequences_.insert(sequence);
  return EntityId{lineage_, sequence};
}

EntityIdAllocatorSnapshot EntityIdAllocator::snapshot() const noexcept {
  return EntityIdAllocatorSnapshot{.next_runtime_sequence = next_runtime_sequence_};
}

} // namespace dross
