#include <dross/foundation/version.hpp>

#include <catch2/catch_test_macros.hpp>
#include <string_view>

TEST_CASE("engine version is semantic and deterministic") {
  constexpr dross::SemanticVersion version = dross::engine_version();

  STATIC_REQUIRE(version.major == 0);
  STATIC_REQUIRE(version.minor == 1);
  STATIC_REQUIRE(version.patch == 0);
}

TEST_CASE("build information excludes volatile values") {
  const std::string_view info = dross::build_information();

  CHECK(info == "dross-engine 0.1.0 phase-03");
  CHECK(info.find("202") == std::string_view::npos);
  CHECK(info.find(':') == std::string_view::npos);
}
