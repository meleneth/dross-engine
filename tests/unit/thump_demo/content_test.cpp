#include "content.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("ThumpDemo owns a package layered on the Dross engine manifest") {
  const auto manifest = thump_demo::content_manifest();

  REQUIRE(manifest.size() == 2);
  CHECK(manifest[0].package_id.canonical() == "dross:base");
  CHECK(manifest[1].package_id == thump_demo::package_id());
  CHECK(manifest[1].package_id.canonical() == "thump_demo:package");
  CHECK(manifest[1].dependencies.size() == 1);
  CHECK(manifest[1].dependencies.front().canonical() == "dross:base");
  CHECK(thump_demo::room_id().canonical() == "thump_demo:room");
  CHECK(thump_demo::thump_ability_id().canonical() == "thump_demo:thump");
}
