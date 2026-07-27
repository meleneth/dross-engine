#pragma once

#include <dross/foundation/result.hpp>
#include <dross/identity/content_id.hpp>
#include <dross/identity/ids.hpp>

#include <compare>
#include <cstdint>
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace dross {

inline constexpr std::uint32_t random_algorithm_version = 1;

class MasterSeed {
public:
  explicit constexpr MasterSeed(const std::uint64_t value) noexcept : value_{value} {}
  [[nodiscard]] constexpr std::uint64_t value() const noexcept { return value_; }
  [[nodiscard]] constexpr auto operator<=>(const MasterSeed&) const = default;

private:
  std::uint64_t value_;
};

class RandomStreamId {
public:
  explicit RandomStreamId(ContentId value) : value_{std::move(value)} {}
  [[nodiscard]] const ContentId& content_id() const noexcept { return value_; }
  [[nodiscard]] auto operator<=>(const RandomStreamId&) const = default;

private:
  ContentId value_;
};

struct RandomSeedMaterial {
  std::uint64_t state_low;
  std::uint64_t state_high;
  std::uint64_t sequence_low;
  std::uint64_t sequence_high;

  [[nodiscard]] auto operator<=>(const RandomSeedMaterial&) const = default;
};

struct RationalChance {
  std::uint64_t numerator;
  std::uint64_t denominator;
};

enum class RandomError : std::uint8_t {
  invalid_bound,
  invalid_range,
  invalid_probability,
};

struct RandomStreamSnapshot {
  RandomStreamId id;
  RandomSeedMaterial seed_material;
  std::uint64_t state_advance_low;
  std::uint64_t state_advance_high;
  std::uint64_t call_count;

  [[nodiscard]] bool operator==(const RandomStreamSnapshot&) const = default;
};

struct RandomHubSnapshot {
  MasterSeed master_seed;
  std::uint32_t algorithm_version;
  std::vector<RandomStreamSnapshot> streams;

  [[nodiscard]] bool operator==(const RandomHubSnapshot&) const = default;
};

enum class RandomRestoreError : std::uint8_t {
  wrong_master_seed,
  wrong_algorithm_version,
  duplicate_stream,
  invalid_seed_material,
};

[[nodiscard]] RandomSeedMaterial derive_random_stream(MasterSeed master_seed,
                                                      const RandomStreamId& stream_id);
[[nodiscard]] RandomStreamId script_child_stream_id(const ContentId& module, EntityId scope);

class RandomHub;

class RandomStream {
public:
  ~RandomStream();
  RandomStream(RandomStream&&) noexcept;
  RandomStream& operator=(RandomStream&&) noexcept;
  RandomStream(const RandomStream&) = delete;
  RandomStream& operator=(const RandomStream&) = delete;

  [[nodiscard]] std::uint64_t next_u64();
  [[nodiscard]] Result<std::uint64_t, RandomError> bounded_u64(std::uint64_t upper_exclusive);
  [[nodiscard]] Result<std::int64_t, RandomError> uniform_int(std::int64_t minimum,
                                                              std::int64_t maximum);
  [[nodiscard]] Result<bool, RandomError> chance(RationalChance probability);

  template <class Value> void shuffle(std::vector<Value>& values) {
    for (std::size_t remaining = values.size(); remaining > 1; --remaining) {
      const auto selected = bounded_u64(static_cast<std::uint64_t>(remaining)).value();
      using std::swap;
      swap(values[remaining - 1], values[static_cast<std::size_t>(selected)]);
    }
  }

  [[nodiscard]] std::uint64_t call_count() const noexcept;

private:
  friend class RandomHub;
  struct Impl;
  RandomStream(RandomStreamId stream_id, RandomSeedMaterial material);
  [[nodiscard]] RandomStreamSnapshot snapshot() const;
  [[nodiscard]] Result<void, RandomRestoreError> restore(const RandomStreamSnapshot& snapshot);

  std::unique_ptr<Impl> impl_;
};

class RandomHub {
public:
  explicit RandomHub(MasterSeed master_seed);
  ~RandomHub();
  RandomHub(RandomHub&&) noexcept;
  RandomHub& operator=(RandomHub&&) noexcept;
  RandomHub(const RandomHub&) = delete;
  RandomHub& operator=(const RandomHub&) = delete;

  [[nodiscard]] RandomStream& stream(RandomStreamId stream_id);
  [[nodiscard]] RandomHubSnapshot snapshot() const;
  [[nodiscard]] Result<void, RandomRestoreError> restore(const RandomHubSnapshot& snapshot);
  [[nodiscard]] MasterSeed master_seed() const noexcept;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace dross
