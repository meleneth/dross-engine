#include <dross/foundation/byte_codec.hpp>
#include <dross/identity/ids.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <sstream>
#include <type_traits>

static_assert(!std::is_convertible_v<dross::EntityId, dross::CommandId>);
static_assert(!std::is_convertible_v<dross::WorldInstanceId, dross::EntityId>);

TEST_CASE("entity IDs order, display, and round trip canonically") {
  constexpr dross::EntityId first{7, 41};
  constexpr dross::EntityId second{7, 42};
  STATIC_REQUIRE(first < second);

  std::ostringstream output;
  output.imbue(std::locale::classic());
  output << second;
  CHECK(output.str() == "entity:7:42");

  dross::ByteWriter writer;
  writer.write(second);
  const std::array expected{std::byte{0x07}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
                            std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
                            std::byte{0x2A}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
                            std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};
  CHECK(std::ranges::equal(writer.bytes(), expected));

  dross::ByteReader reader{writer.bytes()};
  const auto decoded = reader.read_entity_id();
  REQUIRE(decoded);
  CHECK(*decoded == second);
}
