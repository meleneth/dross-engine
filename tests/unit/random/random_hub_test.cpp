#include <dross/random/random_hub.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <vector>

namespace {

dross::RandomStreamId stream_id(const char* value) {
  return dross::RandomStreamId{dross::ContentId::parse(value).value()};
}

} // namespace

TEST_CASE("named RandomHub stream locks the pcg64 golden vector") {
  dross::RandomHub hub{dross::MasterSeed{12345}};
  auto& stream = hub.stream(stream_id("dross:combat"));

  const std::array actual{
      stream.next_u64(), stream.next_u64(), stream.next_u64(),
      stream.next_u64(), stream.next_u64(), stream.next_u64(),
  };

  CHECK(actual == std::array<std::uint64_t, 6>{
                      2585938165455259516ULL,
                      9602076025695250930ULL,
                      7803136475761530990ULL,
                      2091401679913406982ULL,
                      5041201914692415063ULL,
                      14139217093399497297ULL,
                  });
}

TEST_CASE("stream derivation has versioned stable golden material") {
  const auto material =
      dross::derive_random_stream(dross::MasterSeed{12345}, stream_id("dross:combat"));

  CHECK(dross::random_algorithm_version == 1);
  CHECK(material == dross::RandomSeedMaterial{
                        .state_low = 11343461081317094103ULL,
                        .state_high = 597904880811808718ULL,
                        .sequence_low = 9464481265071737773ULL,
                        .sequence_high = 9727432548179850042ULL,
                    });
}

TEST_CASE("named streams are independent and created in canonical snapshots") {
  dross::RandomHub first{dross::MasterSeed{88}};
  dross::RandomHub second{dross::MasterSeed{88}};
  auto& first_combat = first.stream(stream_id("dross:combat"));
  static_cast<void>(first_combat.next_u64());
  static_cast<void>(first_combat.next_u64());

  const auto first_loot = first.stream(stream_id("dross:loot")).next_u64();
  const auto second_loot = second.stream(stream_id("dross:loot")).next_u64();

  CHECK(first_loot == second_loot);
  const auto snapshot = first.snapshot();
  REQUIRE(snapshot.streams.size() == 2);
  CHECK(snapshot.streams[0].id == stream_id("dross:combat"));
  CHECK(snapshot.streams[1].id == stream_id("dross:loot"));
}

TEST_CASE("integer and rational sampling reject invalid contracts") {
  dross::RandomHub hub{dross::MasterSeed{9}};
  auto& stream = hub.stream(stream_id("dross:encounters"));

  CHECK_FALSE(stream.bounded_u64(0));
  CHECK(stream.bounded_u64(1).value() == 0);
  CHECK_FALSE(stream.uniform_int(4, -2));
  const auto integer = stream.uniform_int(-4, 4);
  REQUIRE(integer);
  CHECK(*integer >= -4);
  CHECK(*integer <= 4);
  CHECK_FALSE(stream.chance(dross::RationalChance{.numerator = 1, .denominator = 0}));
  CHECK_FALSE(stream.chance(dross::RationalChance{.numerator = 2, .denominator = 1}));
  CHECK(stream.chance(dross::RationalChance{.numerator = 0, .denominator = 1}).value() == false);
  CHECK(stream.chance(dross::RationalChance{.numerator = 1, .denominator = 1}).value() == true);
}

TEST_CASE("Dross shuffle has a deterministic vector without std shuffle") {
  dross::RandomHub hub{dross::MasterSeed{12345}};
  auto& stream = hub.stream(stream_id("dross:initiative"));
  std::vector values{0, 1, 2, 3, 4, 5, 6, 7};

  stream.shuffle(values);

  CHECK(values == std::vector{2, 4, 0, 3, 5, 6, 1, 7});
}

TEST_CASE("RandomHub snapshot restore resumes every stream") {
  dross::RandomHub hub{dross::MasterSeed{77}};
  auto& combat = hub.stream(stream_id("dross:combat"));
  auto& scripts = hub.stream(stream_id("dross:world_scripts"));
  static_cast<void>(combat.next_u64());
  static_cast<void>(scripts.next_u64());
  const auto snapshot = hub.snapshot();
  const auto expected_combat = combat.next_u64();
  const auto expected_scripts = scripts.next_u64();

  REQUIRE(hub.restore(snapshot));

  CHECK(hub.stream(stream_id("dross:combat")).next_u64() == expected_combat);
  CHECK(hub.stream(stream_id("dross:world_scripts")).next_u64() == expected_scripts);
}

TEST_CASE("script child stream IDs depend on module and stable scope identity") {
  const auto module = dross::ContentId::parse("mousecult:ritual").value();
  const auto first = dross::script_child_stream_id(module, dross::EntityId{7, 11});
  const auto repeated = dross::script_child_stream_id(module, dross::EntityId{7, 11});
  const auto other_scope = dross::script_child_stream_id(module, dross::EntityId{7, 12});

  CHECK(first == repeated);
  CHECK(first != other_scope);
}

TEST_CASE("changed master seed changes the known world script roll") {
  dross::RandomHub recorded{dross::MasterSeed{12345}};
  dross::RandomHub changed{dross::MasterSeed{1}};
  const auto probability = dross::RationalChance{.numerator = 1, .denominator = 2};

  CHECK(recorded.stream(stream_id("dross:world_scripts")).chance(probability).value());
  CHECK_FALSE(changed.stream(stream_id("dross:world_scripts")).chance(probability).value());
}
