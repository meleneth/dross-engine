#include <dross/random/random_hub.hpp>

#include <blake3.h>
#include <pcg_random.hpp>

#include <array>
#include <limits>
#include <map>
#include <utility>

namespace dross {
namespace {

using pcg_extras::pcg128_t;

constexpr std::string_view derivation_domain{"dross-random-v1"};
constexpr unsigned int bits_per_u64 = 64U;
constexpr std::size_t u64_bytes = sizeof(std::uint64_t);
constexpr std::size_t child_digest_bytes = 16U;
constexpr std::size_t hex_characters_per_byte = 2U;
constexpr std::uint8_t low_nibble_mask = 0x0FU;
constexpr std::string_view hexadecimal{"0123456789abcdef"};
constexpr std::uint64_t signed_order_bit = std::uint64_t{1} << (bits_per_u64 - 1U);

void update_u64(blake3_hasher& hasher, const std::uint64_t value) {
  std::array<std::uint8_t, sizeof(value)> bytes{};
  for (std::size_t index = 0; index < bytes.size(); ++index) {
    const auto shift = static_cast<unsigned int>(index * 8U);
    bytes[index] = static_cast<std::uint8_t>(value >> shift);
  }
  blake3_hasher_update(&hasher, bytes.data(), bytes.size());
}

std::uint64_t read_u64(const std::uint8_t* bytes) {
  std::uint64_t value{0};
  for (std::size_t index = 0; index < sizeof(value); ++index) {
    const auto shift = static_cast<unsigned int>(index * 8U);
    value |= static_cast<std::uint64_t>(bytes[index]) << shift;
  }
  return value;
}

pcg128_t make_u128(const std::uint64_t low, const std::uint64_t high) {
  return (pcg128_t{high} << bits_per_u64) | pcg128_t{low};
}

std::uint64_t low_u64(const pcg128_t value) { return static_cast<std::uint64_t>(value); }

std::uint64_t high_u64(const pcg128_t value) {
  return static_cast<std::uint64_t>(value >> bits_per_u64);
}

std::uint64_t ordered_signed(const std::int64_t value) {
  return static_cast<std::uint64_t>(value) ^ signed_order_bit;
}

std::int64_t signed_from_ordered(const std::uint64_t value) {
  return static_cast<std::int64_t>(value ^ signed_order_bit);
}

} // namespace

struct RandomStream::Impl {
  Impl(RandomStreamId stream_id, const RandomSeedMaterial material)
      : id{std::move(stream_id)}, seed_material{material},
        engine{make_u128(material.state_low, material.state_high),
               make_u128(material.sequence_low, material.sequence_high)},
        initial_engine{engine} {}

  RandomStreamId id;
  RandomSeedMaterial seed_material;
  pcg64 engine;
  pcg64 initial_engine;
  std::uint64_t calls{0};
};

struct RandomHub::Impl {
  explicit Impl(const MasterSeed seed) : master_seed{seed} {}

  MasterSeed master_seed;
  std::map<RandomStreamId, RandomStream> streams;
};

RandomSeedMaterial derive_random_stream(const MasterSeed master_seed,
                                        const RandomStreamId& stream_id) {
  blake3_hasher hasher;
  blake3_hasher_init(&hasher);
  blake3_hasher_update(&hasher, derivation_domain.data(), derivation_domain.size());
  constexpr std::uint8_t separator = 0;
  blake3_hasher_update(&hasher, &separator, sizeof(separator));
  update_u64(hasher, master_seed.value());
  const auto canonical = stream_id.content_id().canonical();
  blake3_hasher_update(&hasher, canonical.data(), canonical.size());

  std::array<std::uint8_t, BLAKE3_OUT_LEN> digest{};
  blake3_hasher_finalize(&hasher, digest.data(), digest.size());
  return RandomSeedMaterial{
      .state_low = read_u64(digest.data()),
      .state_high = read_u64(digest.data() + u64_bytes),
      .sequence_low = read_u64(digest.data() + (u64_bytes * 2U)),
      .sequence_high = read_u64(digest.data() + (u64_bytes * 3U)),
  };
}

RandomStreamId script_child_stream_id(const ContentId& module, const EntityId scope) {
  blake3_hasher hasher;
  blake3_hasher_init(&hasher);
  constexpr std::string_view domain{"dross-script-stream-v1"};
  blake3_hasher_update(&hasher, domain.data(), domain.size());
  const auto canonical = module.canonical();
  blake3_hasher_update(&hasher, canonical.data(), canonical.size());
  update_u64(hasher, scope.lineage());
  update_u64(hasher, scope.sequence());
  std::array<std::uint8_t, child_digest_bytes> digest{};
  blake3_hasher_finalize(&hasher, digest.data(), digest.size());

  std::string value{"dross:script_"};
  value.reserve(value.size() + (digest.size() * hex_characters_per_byte));
  for (const auto byte : digest) {
    value.push_back(hexadecimal[byte >> 4U]);
    value.push_back(hexadecimal[byte & low_nibble_mask]);
  }
  return RandomStreamId{ContentId::parse(value).value()};
}

RandomStream::RandomStream(RandomStreamId stream_id, const RandomSeedMaterial material)
    : impl_{std::make_unique<Impl>(std::move(stream_id), material)} {}

RandomStream::~RandomStream() = default;
RandomStream::RandomStream(RandomStream&&) noexcept = default;
RandomStream& RandomStream::operator=(RandomStream&&) noexcept = default;

std::uint64_t RandomStream::next_u64() {
  ++impl_->calls;
  return impl_->engine();
}

Result<std::uint64_t, RandomError> RandomStream::bounded_u64(const std::uint64_t upper_exclusive) {
  if (upper_exclusive == 0) {
    return tl::unexpected{RandomError::invalid_bound};
  }
  ++impl_->calls;
  return impl_->engine(upper_exclusive);
}

Result<std::int64_t, RandomError> RandomStream::uniform_int(const std::int64_t minimum,
                                                            const std::int64_t maximum) {
  if (minimum > maximum) {
    return tl::unexpected{RandomError::invalid_range};
  }
  const auto ordered_minimum = ordered_signed(minimum);
  const auto ordered_maximum = ordered_signed(maximum);
  const auto span = ordered_maximum - ordered_minimum + 1U;
  if (span == 0) {
    return signed_from_ordered(next_u64());
  }
  const auto offset = bounded_u64(span);
  return signed_from_ordered(ordered_minimum + *offset);
}

Result<bool, RandomError> RandomStream::chance(const RationalChance probability) {
  if (probability.denominator == 0 || probability.numerator > probability.denominator) {
    return tl::unexpected{RandomError::invalid_probability};
  }
  if (probability.numerator == 0) {
    return false;
  }
  if (probability.numerator == probability.denominator) {
    return true;
  }
  return *bounded_u64(probability.denominator) < probability.numerator;
}

std::uint64_t RandomStream::call_count() const noexcept { return impl_->calls; }

RandomStreamSnapshot RandomStream::snapshot() const {
  const auto advance = impl_->engine - impl_->initial_engine;
  return RandomStreamSnapshot{
      .id = impl_->id,
      .seed_material = impl_->seed_material,
      .state_advance_low = low_u64(advance),
      .state_advance_high = high_u64(advance),
      .call_count = impl_->calls,
  };
}

Result<void, RandomRestoreError> RandomStream::restore(const RandomStreamSnapshot& snapshot_value) {
  if (snapshot_value.seed_material != impl_->seed_material || snapshot_value.id != impl_->id) {
    return tl::unexpected{RandomRestoreError::invalid_seed_material};
  }
  impl_->engine = impl_->initial_engine;
  impl_->engine.advance(
      make_u128(snapshot_value.state_advance_low, snapshot_value.state_advance_high));
  impl_->calls = snapshot_value.call_count;
  return {};
}

RandomHub::RandomHub(const MasterSeed master_seed) : impl_{std::make_unique<Impl>(master_seed)} {}

RandomHub::~RandomHub() = default;
RandomHub::RandomHub(RandomHub&&) noexcept = default;
RandomHub& RandomHub::operator=(RandomHub&&) noexcept = default;

RandomStream& RandomHub::stream(RandomStreamId stream_id) {
  const auto found = impl_->streams.find(stream_id);
  if (found != impl_->streams.end()) {
    return found->second;
  }
  const auto material = derive_random_stream(impl_->master_seed, stream_id);
  RandomStream value{stream_id, material};
  return impl_->streams.emplace(std::move(stream_id), std::move(value)).first->second;
}

RandomHubSnapshot RandomHub::snapshot() const {
  RandomHubSnapshot result{
      .master_seed = impl_->master_seed,
      .algorithm_version = random_algorithm_version,
      .streams = {},
  };
  result.streams.reserve(impl_->streams.size());
  for (const auto& [id, stream_value] : impl_->streams) {
    static_cast<void>(id);
    result.streams.push_back(stream_value.snapshot());
  }
  return result;
}

Result<void, RandomRestoreError> RandomHub::restore(const RandomHubSnapshot& snapshot_value) {
  if (snapshot_value.master_seed != impl_->master_seed) {
    return tl::unexpected{RandomRestoreError::wrong_master_seed};
  }
  if (snapshot_value.algorithm_version != random_algorithm_version) {
    return tl::unexpected{RandomRestoreError::wrong_algorithm_version};
  }

  std::map<RandomStreamId, RandomStream> restored;
  for (const auto& stream_snapshot : snapshot_value.streams) {
    if (restored.contains(stream_snapshot.id)) {
      return tl::unexpected{RandomRestoreError::duplicate_stream};
    }
    if (derive_random_stream(impl_->master_seed, stream_snapshot.id) !=
        stream_snapshot.seed_material) {
      return tl::unexpected{RandomRestoreError::invalid_seed_material};
    }
    RandomStream value{stream_snapshot.id, stream_snapshot.seed_material};
    const auto valid = value.restore(stream_snapshot);
    if (!valid) {
      return valid;
    }
    restored.emplace(stream_snapshot.id, std::move(value));
  }
  impl_->streams = std::move(restored);
  return {};
}

MasterSeed RandomHub::master_seed() const noexcept { return impl_->master_seed; }

} // namespace dross
