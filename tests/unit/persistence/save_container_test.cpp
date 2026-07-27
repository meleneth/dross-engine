#include <dross/persistence/save_container.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>

namespace {

dross::ContentId content_id(const char* value) { return dross::ContentId::parse(value).value(); }

dross::SaveContainer container_with(std::vector<dross::ComponentRecord> records) {
  dross::CheckpointHash map_hash{};
  map_hash.front() = 0x42U;
  return dross::SaveContainer{
      .header =
          dross::SaveHeader{
              .container_version = 1,
              .simulation_schema_version = 1,
              .engine_version = dross::engine_version(),
              .ticks_per_second = 30,
              .current_tick = dross::Tick{7},
              .map_id = content_id("dross:arena"),
              .map_hash = map_hash,
          },
      .components = std::move(records),
  };
}

} // namespace

TEST_CASE("component codec registry rejects duplicate stable type IDs") {
  dross::ComponentCodecRegistry registry;
  const auto codec = dross::ComponentCodecDescriptor{
      .type_id = content_id("dross:persistent_identity"),
      .current_version = 1,
  };

  REQUIRE(registry.register_codec(codec));
  const auto duplicate = registry.register_codec(codec);

  REQUIRE_FALSE(duplicate);
  CHECK(duplicate.error() == dross::CodecRegistrationError::duplicate_type_id);
}

TEST_CASE("current component codecs are explicit and canonically ordered") {
  dross::ComponentCodecRegistry registry;
  REQUIRE(dross::register_current_component_codecs(registry));

  const auto codecs = registry.descriptors();
  REQUIRE(codecs.size() == 2);
  CHECK(codecs[0].type_id == content_id("dross:hex_pose"));
  CHECK(codecs[0].current_version == 1);
  CHECK(codecs[1].type_id == content_id("dross:persistent_identity"));
  CHECK(codecs[1].current_version == 1);
}

TEST_CASE("equivalent save records encode to byte-identical canonical containers") {
  const auto identity = dross::ComponentRecord{
      .type_id = content_id("dross:persistent_identity"),
      .version = 1,
      .entity = dross::EntityId{9, 1},
      .payload = {std::byte{0x01}, std::byte{0x02}},
  };
  const auto pose = dross::ComponentRecord{
      .type_id = content_id("dross:hex_pose"),
      .version = 1,
      .entity = dross::EntityId{9, 1},
      .payload = {std::byte{0x03}},
  };

  const auto first = dross::encode_save_container(container_with({identity, pose}));
  const auto second = dross::encode_save_container(container_with({pose, identity}));

  CHECK(first == second);
}

TEST_CASE("save container round trip preserves required header and component records") {
  const auto record = dross::ComponentRecord{
      .type_id = content_id("dross:persistent_identity"),
      .version = 1,
      .entity = dross::EntityId{9, 1},
      .payload = {std::byte{0x01}, std::byte{0x02}},
  };
  const auto expected = container_with({record});

  const auto decoded = dross::decode_save_container(dross::encode_save_container(expected));

  REQUIRE(decoded);
  CHECK(*decoded == expected);
}

TEST_CASE("save decoder rejects truncated and malformed container bytes") {
  const auto encoded = dross::encode_save_container(container_with({}));
  const std::array malformed{std::byte{0xFF}, std::byte{0x00}};

  CHECK_FALSE(
      dross::decode_save_container(std::span<const std::byte>{encoded}.first(encoded.size() - 1)));
  CHECK_FALSE(dross::decode_save_container(malformed));
}
