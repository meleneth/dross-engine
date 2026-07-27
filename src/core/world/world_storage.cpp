#include <dross/world/world_storage.hpp>

#include <entt/entity/registry.hpp>

#include <algorithm>
#include <map>
#include <utility>

namespace dross {

struct PlacementPose {
  std::optional<HexPose> value;
};

struct WorldStorage::Impl {
  explicit Impl(const WorldConfig& config)
      : lineage{config.lineage}, instance_id{config.instance_id},
        allocator{config.lineage, EntityIdAllocatorSnapshot{.next_runtime_sequence =
                                                                config.next_runtime_sequence}} {}

  std::uint64_t lineage;
  WorldInstanceId instance_id;
  EntityIdAllocator allocator;
  entt::registry registry;
  std::map<EntityId, entt::entity> entities;
  std::map<ContentId, EntityId> aliases;
};

SpawnPlan SpawnPlan::runtime(std::optional<EntityAlias> alias) {
  return SpawnPlan{.authored_sequence = std::nullopt, .alias = std::move(alias)};
}

SpawnPlan SpawnPlan::authored(const std::uint64_t sequence, std::optional<EntityAlias> alias) {
  return SpawnPlan{.authored_sequence = sequence, .alias = std::move(alias)};
}

WorldStorage::WorldStorage(WorldConfig config) : impl_{std::make_unique<Impl>(config)} {}

WorldStorage::~WorldStorage() = default;

EntityIdAllocatorSnapshot WorldStorage::allocator_snapshot() const noexcept {
  return impl_->allocator.snapshot();
}

bool WorldRead::valid(const EntityRef entity) const {
  return entity.world_instance() == storage_->impl_->instance_id &&
         storage_->impl_->entities.contains(entity.id());
}

Result<PersistentIdentity, EntityLookupError> WorldRead::lookup(const EntityRef entity) const {
  if (entity.world_instance() != storage_->impl_->instance_id) {
    return tl::unexpected{
        EntityLookupError{.reason = EntityLookupErrorReason::wrong_world_instance}};
  }
  const auto found = storage_->impl_->entities.find(entity.id());
  if (found == storage_->impl_->entities.end()) {
    return tl::unexpected{EntityLookupError{.reason = EntityLookupErrorReason::entity_not_found}};
  }
  return storage_->impl_->registry.get<PersistentIdentity>(found->second);
}

Result<PersistentIdentity, EntityLookupError> WorldRead::identity(const EntityRef entity) const {
  return lookup(entity);
}

Result<HexPose, EntityLookupError> WorldRead::pose(const EntityRef entity) const {
  if (entity.world_instance() != storage_->impl_->instance_id) {
    return tl::unexpected{
        EntityLookupError{.reason = EntityLookupErrorReason::wrong_world_instance}};
  }
  const auto found = storage_->impl_->entities.find(entity.id());
  if (found == storage_->impl_->entities.end()) {
    return tl::unexpected{EntityLookupError{.reason = EntityLookupErrorReason::entity_not_found}};
  }
  const auto& pose = storage_->impl_->registry.get<const PlacementPose>(found->second);
  if (!pose.value) {
    return tl::unexpected{
        EntityLookupError{.reason = EntityLookupErrorReason::capability_not_present}};
  }
  return *pose.value;
}

std::optional<EntityRef> WorldRead::find(const EntityId entity_id) const {
  if (!storage_->impl_->entities.contains(entity_id)) {
    return std::nullopt;
  }
  return EntityRef{storage_->impl_->instance_id, entity_id};
}

std::optional<EntityRef> WorldRead::find(const EntityAlias& alias) const {
  const auto found = storage_->impl_->aliases.find(alias.content_id());
  if (found == storage_->impl_->aliases.end()) {
    return std::nullopt;
  }
  return EntityRef{storage_->impl_->instance_id, found->second};
}

std::size_t WorldRead::entity_count() const { return storage_->impl_->entities.size(); }

std::vector<EntityId> WorldRead::entity_ids() const {
  std::vector<EntityId> result;
  result.reserve(storage_->impl_->entities.size());
  const auto view = storage_->impl_->registry.view<const PersistentIdentity>();
  for (const auto handle : view) {
    result.push_back(view.get<const PersistentIdentity>(handle).id);
  }
  return result;
}

std::vector<EntityId> WorldRead::stable_entity_ids() const {
  auto result = entity_ids();
  std::ranges::sort(result);
  return result;
}

Result<EntityRef, SpawnError> WorldWrite::spawn(const SpawnPlan& plan) {
  if (plan.alias && storage_->impl_->aliases.contains(plan.alias->content_id())) {
    return tl::unexpected{SpawnError{.reason = SpawnErrorReason::duplicate_alias}};
  }

  Result<EntityId, EntityIdAllocationError> allocated =
      plan.authored_sequence ? storage_->impl_->allocator.reserve_authored(*plan.authored_sequence)
                             : storage_->impl_->allocator.allocate_runtime();
  if (!allocated) {
    if (allocated.error() == EntityIdAllocationError::sequence_exhausted) {
      return tl::unexpected{SpawnError{.reason = SpawnErrorReason::sequence_exhausted}};
    }
    const auto requested = EntityId{storage_->impl_->lineage, *plan.authored_sequence};
    const auto reason = storage_->impl_->entities.contains(requested)
                            ? SpawnErrorReason::duplicate_id
                            : SpawnErrorReason::retired_id;
    return tl::unexpected{SpawnError{.reason = reason}};
  }

  const auto handle = storage_->impl_->registry.create();
  storage_->impl_->registry.emplace<PersistentIdentity>(
      handle, PersistentIdentity{.id = *allocated, .alias = plan.alias});
  storage_->impl_->registry.emplace<PlacementPose>(handle, PlacementPose{});
  storage_->impl_->entities.emplace(*allocated, handle);
  if (plan.alias) {
    storage_->impl_->aliases.emplace(plan.alias->content_id(), *allocated);
  }
  return EntityRef{storage_->impl_->instance_id, *allocated};
}

Result<void, DestroyError> WorldWrite::destroy(const EntityRef entity) {
  if (entity.world_instance() != storage_->impl_->instance_id) {
    return tl::unexpected{DestroyError{.reason = EntityLookupErrorReason::wrong_world_instance}};
  }
  const auto found = storage_->impl_->entities.find(entity.id());
  if (found == storage_->impl_->entities.end()) {
    return tl::unexpected{DestroyError{.reason = EntityLookupErrorReason::entity_not_found}};
  }
  const auto& identity = storage_->impl_->registry.get<const PersistentIdentity>(found->second);
  if (identity.alias) {
    storage_->impl_->aliases.erase(identity.alias->content_id());
  }
  storage_->impl_->registry.destroy(found->second);
  storage_->impl_->entities.erase(found);
  return {};
}

void WorldWrite::commit_pose(const EntityRef entity, HexPose pose) noexcept {
  const auto found = storage_->impl_->entities.find(entity.id());
  storage_->impl_->registry.get<PlacementPose>(found->second).value = std::move(pose);
}

} // namespace dross
