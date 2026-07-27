#pragma once

#include <dross/foundation/result.hpp>
#include <dross/identity/entity_alias.hpp>
#include <dross/identity/entity_id_allocator.hpp>
#include <dross/identity/entity_ref.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace dross {

struct WorldConfig {
  std::uint64_t lineage;
  WorldInstanceId instance_id;
  std::uint64_t next_runtime_sequence{1};
};

struct PersistentIdentity {
  EntityId id;
  std::optional<EntityAlias> alias;

  [[nodiscard]] bool operator==(const PersistentIdentity&) const = default;
};

struct SpawnPlan {
  std::optional<std::uint64_t> authored_sequence;
  std::optional<EntityAlias> alias;

  [[nodiscard]] static SpawnPlan runtime(std::optional<EntityAlias> alias = std::nullopt);
  [[nodiscard]] static SpawnPlan authored(std::uint64_t sequence,
                                          std::optional<EntityAlias> alias = std::nullopt);
};

enum class SpawnErrorReason : std::uint8_t {
  duplicate_id,
  retired_id,
  duplicate_alias,
  sequence_exhausted,
};

struct SpawnError {
  SpawnErrorReason reason;
};

enum class EntityLookupErrorReason : std::uint8_t {
  wrong_world_instance,
  entity_not_found,
};

struct EntityLookupError {
  EntityLookupErrorReason reason;
};

using DestroyError = EntityLookupError;

class WorldStorage;

class WorldRead {
public:
  [[nodiscard]] bool valid(EntityRef entity) const;
  [[nodiscard]] Result<PersistentIdentity, EntityLookupError> lookup(EntityRef entity) const;
  [[nodiscard]] Result<PersistentIdentity, EntityLookupError> identity(EntityRef entity) const;
  [[nodiscard]] std::optional<EntityRef> find(EntityId id) const;
  [[nodiscard]] std::optional<EntityRef> find(const EntityAlias& alias) const;
  [[nodiscard]] std::size_t entity_count() const;
  [[nodiscard]] std::vector<EntityId> entity_ids() const;
  [[nodiscard]] std::vector<EntityId> stable_entity_ids() const;

private:
  friend class WorldStorage;
  explicit WorldRead(const WorldStorage& storage) noexcept : storage_{&storage} {}
  const WorldStorage* storage_;
};

class WorldWrite {
public:
  [[nodiscard]] Result<EntityRef, SpawnError> spawn(const SpawnPlan& plan);
  [[nodiscard]] Result<void, DestroyError> destroy(EntityRef entity);

private:
  friend class WorldStorage;
  explicit WorldWrite(WorldStorage& storage) noexcept : storage_{&storage} {}
  WorldStorage* storage_;
};

class WorldStorage {
public:
  explicit WorldStorage(WorldConfig config);
  ~WorldStorage();
  WorldStorage(const WorldStorage&) = delete;
  WorldStorage& operator=(const WorldStorage&) = delete;

  [[nodiscard]] WorldRead read() const noexcept { return WorldRead{*this}; }
  [[nodiscard]] WorldWrite write() noexcept { return WorldWrite{*this}; }
  [[nodiscard]] EntityIdAllocatorSnapshot allocator_snapshot() const noexcept;

private:
  friend class WorldRead;
  friend class WorldWrite;
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace dross
