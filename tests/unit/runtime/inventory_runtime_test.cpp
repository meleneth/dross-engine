#include <dross/runtime/inventory_runtime.hpp>

#include <catch2/catch_test_macros.hpp>

#include <limits>
#include <string>
#include <vector>

namespace {

dross::ContentId id(const char* value) { return dross::ContentId::parse(value).value(); }

constexpr dross::WorldInstanceId world{7};
constexpr dross::EntityId player_id{52, 1};
constexpr dross::EntityId mouse_id{52, 2};
constexpr dross::EntityRef player{world, player_id};

class RecordingInventoryEvents final : public dross::InventoryRuntime::EventSink {
public:
  void publish(const dross::inventory::ItemGranted& event) override {
    calls.push_back("grant/" + std::string{event.item.canonical()} + "/" +
                    std::to_string(event.count) + "/" + std::to_string(event.new_count));
  }

  void publish(const dross::inventory::ItemRemoved& event) override {
    calls.push_back("remove/" + std::string{event.item.canonical()} + "/" +
                    std::to_string(event.count) + "/" + std::to_string(event.new_count));
  }

  std::vector<std::string> calls;
};

} // namespace

TEST_CASE("inventory grants removes and publishes immutable resulting counts") {
  RecordingInventoryEvents events;
  dross::InventoryRuntime inventory{world, {player_id}, &events};
  const auto tail = id("thump_demo:mouse_tail");

  REQUIRE(inventory.handle(dross::inventory::GrantItem{.owner = player, .item = tail, .count = 2}));
  CHECK(inventory.count(player, tail) == 2);
  CHECK(inventory.has(player, tail, 2));
  REQUIRE(
      inventory.handle(dross::inventory::RemoveItem{.owner = player, .item = tail, .count = 1}));
  CHECK(inventory.count(player, tail) == 1);
  CHECK(events.calls == std::vector<std::string>{"grant/thump_demo:mouse_tail/2/2",
                                                 "remove/thump_demo:mouse_tail/1/1"});
}

TEST_CASE("inventory rejection leaves authoritative state and events unchanged") {
  RecordingInventoryEvents events;
  dross::InventoryRuntime inventory{world, {player_id}, &events};
  const auto tail = id("thump_demo:mouse_tail");
  REQUIRE(inventory.handle(dross::inventory::GrantItem{.owner = player, .item = tail, .count = 1}));
  const auto before = inventory.snapshot();
  const auto event_count = events.calls.size();

  const auto zero =
      inventory.handle(dross::inventory::GrantItem{.owner = player, .item = tail, .count = 0});
  const auto unknown = inventory.handle(dross::inventory::GrantItem{
      .owner = dross::EntityRef{world, mouse_id}, .item = tail, .count = 1});
  const auto foreign = inventory.handle(dross::inventory::RemoveItem{
      .owner = dross::EntityRef{dross::WorldInstanceId{8}, player_id}, .item = tail, .count = 1});
  const auto insufficient =
      inventory.handle(dross::inventory::RemoveItem{.owner = player, .item = tail, .count = 2});

  REQUIRE_FALSE(zero);
  CHECK(zero.error() == dross::InventoryRejection::invalid_count);
  REQUIRE_FALSE(unknown);
  CHECK(unknown.error() == dross::InventoryRejection::unknown_owner);
  REQUIRE_FALSE(foreign);
  CHECK(foreign.error() == dross::InventoryRejection::wrong_world);
  REQUIRE_FALSE(insufficient);
  CHECK(insufficient.error() == dross::InventoryRejection::insufficient_quantity);
  CHECK(inventory.snapshot() == before);
  CHECK(events.calls.size() == event_count);
}

TEST_CASE("inventory snapshots are canonical and restore through the durable codec") {
  dross::InventoryRuntime first{world, {player_id, mouse_id}};
  dross::InventoryRuntime second{world, {mouse_id, player_id}};
  const auto tail = id("thump_demo:mouse_tail");
  const auto key = id("thump_demo:caretaker_key");
  REQUIRE(first.handle(dross::inventory::GrantItem{.owner = player, .item = tail, .count = 1}));
  REQUIRE(first.handle(dross::inventory::GrantItem{.owner = player, .item = key, .count = 2}));
  REQUIRE(second.handle(dross::inventory::GrantItem{.owner = player, .item = key, .count = 2}));
  REQUIRE(second.handle(dross::inventory::GrantItem{.owner = player, .item = tail, .count = 1}));
  CHECK(first.snapshot() == second.snapshot());

  dross::ByteWriter writer;
  dross::encode_inventory_snapshot(writer, first.snapshot());
  dross::ByteReader reader{writer.bytes()};
  const auto decoded = dross::decode_inventory_snapshot(reader);
  REQUIRE(decoded);
  CHECK(reader.remaining() == 0);

  dross::InventoryRuntime restored{world, {player_id, mouse_id}};
  REQUIRE(restored.restore(*decoded));
  CHECK(restored.snapshot() == first.snapshot());
  CHECK(restored.count(player, key) == 2);
}

TEST_CASE("inventory rejects overflow and malformed snapshots without mutation") {
  dross::InventoryRuntime inventory{world, {player_id}};
  const auto tail = id("thump_demo:mouse_tail");
  REQUIRE(inventory.handle(dross::inventory::GrantItem{
      .owner = player, .item = tail, .count = std::numeric_limits<std::uint32_t>::max()}));
  const auto before = inventory.snapshot();
  const auto overflow =
      inventory.handle(dross::inventory::GrantItem{.owner = player, .item = tail, .count = 1});
  REQUIRE_FALSE(overflow);
  CHECK(overflow.error() == dross::InventoryRejection::quantity_overflow);

  auto malformed = before;
  malformed.entries.push_back(malformed.entries.front());
  CHECK_FALSE(inventory.restore(malformed));
  CHECK(inventory.snapshot() == before);
}
