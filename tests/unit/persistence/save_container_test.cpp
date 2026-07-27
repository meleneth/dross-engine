#include <dross/persistence/save_container.hpp>

#include <dross/foundation/byte_codec.hpp>
#include <dross/generated/schema_codec.hpp>

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
              .world_lineage = 9,
              .allocator = dross::EntityIdAllocatorSnapshot{4},
              .map_id = content_id("dross:arena"),
              .map_hash = map_hash,
          },
      .components = std::move(records),
  };
}

std::vector<std::byte> identity_payload(const std::optional<dross::ContentId>& alias) {
  dross::ByteWriter writer;
  writer.write_u16(alias ? 1U : 0U);
  if (alias) {
    writer.write(*alias);
  }
  return {writer.bytes().begin(), writer.bytes().end()};
}

std::vector<std::byte> pose_payload(const dross::HexPose& pose) {
  dross::ByteWriter writer;
  dross::generated::encode_hex_pose(writer, pose);
  return {writer.bytes().begin(), writer.bytes().end()};
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

TEST_CASE("persistent identity V0 migrates purely to the current V1 payload") {
  dross::ComponentCodecRegistry registry;
  REQUIRE(dross::register_current_component_codecs(registry));
  const auto legacy = dross::ComponentRecord{
      .type_id = content_id("dross:persistent_identity"),
      .version = 0,
      .entity = dross::EntityId{9, 1},
      .payload = {},
  };

  const auto migrated = registry.migrate_to_current(legacy);

  REQUIRE(migrated);
  CHECK(migrated->version == 1);
  CHECK(migrated->payload == std::vector<std::byte>{
                                 std::byte{0x00},
                                 std::byte{0x00},
                             });
  CHECK(registry.validate(*migrated));
}

TEST_CASE("component migration rejects unknown types and unsupported future versions") {
  dross::ComponentCodecRegistry registry;
  REQUIRE(dross::register_current_component_codecs(registry));
  const auto unknown = dross::ComponentRecord{
      .type_id = content_id("test:unknown"),
      .version = 1,
      .entity = dross::EntityId{9, 1},
      .payload = {},
  };
  auto future = unknown;
  future.type_id = content_id("dross:persistent_identity");
  future.version = 2;

  const auto unknown_result = registry.migrate_to_current(unknown);
  const auto future_result = registry.migrate_to_current(future);

  REQUIRE_FALSE(unknown_result);
  CHECK(unknown_result.error() == dross::ComponentCodecError::unknown_type_id);
  REQUIRE_FALSE(future_result);
  CHECK(future_result.error() == dross::ComponentCodecError::unsupported_version);
}

TEST_CASE("current component validation rejects malformed payloads") {
  dross::ComponentCodecRegistry registry;
  REQUIRE(dross::register_current_component_codecs(registry));
  const auto malformed = dross::ComponentRecord{
      .type_id = content_id("dross:hex_pose"),
      .version = 1,
      .entity = dross::EntityId{9, 1},
      .payload = {std::byte{0x01}},
  };

  const auto validation = registry.validate(malformed);

  REQUIRE_FALSE(validation);
  CHECK(validation.error() == dross::ComponentCodecError::invalid_payload);
}

TEST_CASE("world load plan validates completely before constructing a fresh world") {
  dross::ComponentCodecRegistry registry;
  REQUIRE(dross::register_current_component_codecs(registry));
  const auto entity = dross::EntityId{9, 3};
  const auto alias = content_id("test:loaded_actor");
  const auto pose = dross::HexPose{
      .anchor =
          dross::HexCellId{
              .region = dross::RegionId{content_id("test:region")},
              .coord = dross::HexCoord{.q = 2, .r = -1},
              .layer = 0,
          },
      .facing = dross::HexFacing::southeast,
  };
  auto container = container_with({
      dross::ComponentRecord{
          .type_id = content_id("dross:persistent_identity"),
          .version = 1,
          .entity = entity,
          .payload = identity_payload(alias),
      },
      dross::ComponentRecord{
          .type_id = content_id("dross:hex_pose"),
          .version = 1,
          .entity = entity,
          .payload = pose_payload(pose),
      },
  });

  const auto plan = dross::build_world_load_plan(container, registry, container.header.map_id,
                                                 container.header.map_hash);

  REQUIRE(plan);
  const auto loaded = plan->construct(dross::WorldInstanceId{77});
  REQUIRE(loaded);
  const auto loaded_ref = (*loaded)->read().find(entity);
  REQUIRE(loaded_ref);
  CHECK((*loaded)->read().identity(*loaded_ref)->alias->content_id() == alias);
  CHECK((*loaded)->read().pose(*loaded_ref) == pose);
  CHECK((*loaded)->allocator_snapshot().next_runtime_sequence == 4);
}

TEST_CASE("world load plan rejects map mismatch and orphan components") {
  dross::ComponentCodecRegistry registry;
  REQUIRE(dross::register_current_component_codecs(registry));
  auto container = container_with({
      dross::ComponentRecord{
          .type_id = content_id("dross:hex_pose"),
          .version = 1,
          .entity = dross::EntityId{9, 3},
          .payload = pose_payload(dross::HexPose{
              .anchor =
                  dross::HexCellId{
                      .region = dross::RegionId{content_id("test:region")},
                      .coord = dross::HexCoord{.q = 0, .r = 0},
                      .layer = 0,
                  },
              .facing = dross::HexFacing::east,
          }),
      },
  });
  auto wrong_hash = container.header.map_hash;
  wrong_hash.front() ^= 0xFFU;

  const auto map_mismatch =
      dross::build_world_load_plan(container, registry, container.header.map_id, wrong_hash);
  const auto orphan = dross::build_world_load_plan(container, registry, container.header.map_id,
                                                   container.header.map_hash);

  REQUIRE_FALSE(map_mismatch);
  CHECK(map_mismatch.error() == dross::WorldLoadError::map_mismatch);
  REQUIRE_FALSE(orphan);
  CHECK(orphan.error() == dross::WorldLoadError::missing_identity);
}

TEST_CASE("world component snapshot round trips through a fresh world") {
  dross::WorldStorage original{dross::WorldConfig{
      .lineage = 9,
      .instance_id = dross::WorldInstanceId{1},
  }};
  const auto alias = dross::EntityAlias{content_id("test:snapshot_actor")};
  const auto entity = original.write().spawn(dross::SpawnPlan::runtime(alias)).value();
  const auto pose = dross::HexPose{
      .anchor =
          dross::HexCellId{
              .region = dross::RegionId{content_id("test:region")},
              .coord = dross::HexCoord{.q = -3, .r = 4},
              .layer = 1,
          },
      .facing = dross::HexFacing::northwest,
  };
  original.write().commit_pose(entity, pose);
  auto container = container_with(dross::snapshot_world_components(original));
  container.header.allocator = original.allocator_snapshot();

  dross::ComponentCodecRegistry registry;
  REQUIRE(dross::register_current_component_codecs(registry));
  const auto plan = dross::build_world_load_plan(container, registry, container.header.map_id,
                                                 container.header.map_hash);
  REQUIRE(plan);
  const auto loaded = plan->construct(dross::WorldInstanceId{2});

  REQUIRE(loaded);
  const auto loaded_ref = (*loaded)->read().find(entity.id());
  REQUIRE(loaded_ref);
  CHECK((*loaded)->read().identity(*loaded_ref)->alias == alias);
  CHECK((*loaded)->read().pose(*loaded_ref) == pose);
  CHECK(loaded_ref->world_instance() != entity.world_instance());
}
