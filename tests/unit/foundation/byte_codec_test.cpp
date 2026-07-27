#include <dross/foundation/byte_codec.hpp>
#include <dross/identity/content_id.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

TEST_CASE("byte writer uses fixed-width little-endian encoding") {
  dross::ByteWriter writer;
  writer.write_u16(0x1234);
  writer.write_u32(0x89ABCDEF);
  writer.write_u64(0x0123456789ABCDEF);
  writer.write_string("ok");

  const std::array expected{std::byte{0x34}, std::byte{0x12}, std::byte{0xEF}, std::byte{0xCD},
                            std::byte{0xAB}, std::byte{0x89}, std::byte{0xEF}, std::byte{0xCD},
                            std::byte{0xAB}, std::byte{0x89}, std::byte{0x67}, std::byte{0x45},
                            std::byte{0x23}, std::byte{0x01}, std::byte{0x02}, std::byte{0x00},
                            std::byte{0x00}, std::byte{0x00}, std::byte{'o'},  std::byte{'k'}};
  CHECK(std::ranges::equal(writer.bytes(), expected));
}

TEST_CASE("byte reader round trips validated content IDs") {
  const auto id = dross::ContentId::parse("dross:thump").value();
  dross::ByteWriter writer;
  writer.write(id);

  dross::ByteReader reader{writer.bytes()};
  const auto decoded = reader.read_content_id();

  REQUIRE(decoded);
  CHECK(*decoded == id);
  CHECK(reader.remaining() == 0);
}

TEST_CASE("byte reader rejects truncation and impossible lengths") {
  const std::array truncated{std::byte{0x01}, std::byte{0x02}};
  dross::ByteReader integer_reader{truncated};
  const auto integer = integer_reader.read_u32();
  REQUIRE_FALSE(integer);
  CHECK(integer.error().reason == dross::DecodeErrorReason::truncated);

  const std::array invalid_length{std::byte{0xFF}, std::byte{0xFF}, std::byte{0xFF},
                                  std::byte{0x7F}};
  dross::ByteReader string_reader{invalid_length};
  const auto string = string_reader.read_string();
  REQUIRE_FALSE(string);
  CHECK(string.error().reason == dross::DecodeErrorReason::invalid_length);
}

TEST_CASE("byte reader reports invalid encoded content IDs") {
  dross::ByteWriter writer;
  writer.write_string("Not:canonical");
  dross::ByteReader reader{writer.bytes()};

  const auto decoded = reader.read_content_id();
  REQUIRE_FALSE(decoded);
  CHECK(decoded.error().reason == dross::DecodeErrorReason::invalid_content_id);
}
