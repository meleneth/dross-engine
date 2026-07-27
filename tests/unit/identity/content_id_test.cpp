#include <dross/identity/content_id.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <string_view>

TEST_CASE("content IDs accept the documented canonical ASCII syntax") {
  for (const std::string_view text :
       {"dross:thump", "mouse_cult:sacred-mouse", "a.b:path/to/thing", "n0:x1"}) {
    const auto parsed = dross::ContentId::parse(text);
    REQUIRE(parsed);
    CHECK(parsed->canonical() == text);
  }
}

TEST_CASE("content IDs report the first invalid byte") {
  const auto check_error = [](const std::string_view text, const std::size_t position,
                              const dross::ContentIdErrorReason reason) {
    const auto parsed = dross::ContentId::parse(text);
    REQUIRE_FALSE(parsed);
    CHECK(parsed.error().position == position);
    CHECK(parsed.error().reason == reason);
  };

  check_error("", 0, dross::ContentIdErrorReason::missing_separator);
  check_error("dross", 5, dross::ContentIdErrorReason::missing_separator);
  check_error(":thump", 0, dross::ContentIdErrorReason::empty_namespace);
  check_error("dross:", 6, dross::ContentIdErrorReason::empty_name);
  check_error("Dross:thump", 0, dross::ContentIdErrorReason::invalid_character);
  check_error("dross:two:parts", 9, dross::ContentIdErrorReason::extra_separator);
  check_error("dross:mouse cult", 11, dross::ContentIdErrorReason::invalid_character);
  check_error("dr\xC3\xB6ss:thump", 2, dross::ContentIdErrorReason::invalid_character);
}

TEST_CASE("content IDs have canonical lexical ordering") {
  const auto first = dross::ContentId::parse("dross:mouse").value();
  const auto second = dross::ContentId::parse("dross:thump").value();
  const auto third = dross::ContentId::parse("mousecult:sacred_mouse").value();

  CHECK(first < second);
  CHECK(second < third);
}

TEST_CASE("content ID canonical bytes and BLAKE3 digest are stable") {
  const auto id = dross::ContentId::parse("dross:thump").value();
  constexpr std::array<std::byte, 32> expected{
      std::byte{0x5D}, std::byte{0x5E}, std::byte{0x34}, std::byte{0x24}, std::byte{0x14},
      std::byte{0x0D}, std::byte{0x0A}, std::byte{0xE1}, std::byte{0x9A}, std::byte{0xC4},
      std::byte{0x91}, std::byte{0xEF}, std::byte{0x86}, std::byte{0x62}, std::byte{0xAF},
      std::byte{0x03}, std::byte{0x5A}, std::byte{0x51}, std::byte{0xF6}, std::byte{0xC3},
      std::byte{0x40}, std::byte{0x86}, std::byte{0x87}, std::byte{0x37}, std::byte{0xE9},
      std::byte{0x10}, std::byte{0xA9}, std::byte{0xAB}, std::byte{0x58}, std::byte{0x62},
      std::byte{0x90}, std::byte{0xA8}};

  CHECK(id.stable_hash() == expected);
}
