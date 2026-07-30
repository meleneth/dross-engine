#include <thump_demo/fsm/caretaker_machine.hpp>

#include <array>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("ThumpDemo caretaker follows the mouse-tail interaction lifecycle") {
  thump_demo::CaretakerMachine caretaker;

  CHECK(caretaker.state() == thump_demo::CaretakerState::waiting_for_mouse_contact);
  CHECK_FALSE(caretaker.observe_mouse_tail());
  CHECK(caretaker.contact_mouse());
  CHECK(caretaker.state() == thump_demo::CaretakerState::hunt_assigned);
  CHECK_FALSE(caretaker.contact_mouse());
  CHECK(caretaker.observe_mouse_tail());
  CHECK(caretaker.state() == thump_demo::CaretakerState::waiting_for_tail);
  CHECK_FALSE(caretaker.observe_mouse_tail());
  CHECK(caretaker.hand_in_tail());
  CHECK(caretaker.state() == thump_demo::CaretakerState::settled);
  CHECK_FALSE(caretaker.hand_in_tail());
}

TEST_CASE("ThumpDemo caretaker snapshots and restores every SML state") {
  constexpr std::array states{
      thump_demo::CaretakerState::waiting_for_mouse_contact,
      thump_demo::CaretakerState::hunt_assigned,
      thump_demo::CaretakerState::waiting_for_tail,
      thump_demo::CaretakerState::settled,
  };

  for (const auto state : states) {
    thump_demo::CaretakerMachine caretaker;
    REQUIRE(caretaker.restore(thump_demo::CaretakerSnapshot{.state = state}));
    CHECK(caretaker.snapshot() == thump_demo::CaretakerSnapshot{.state = state});
  }
}

TEST_CASE("ThumpDemo caretaker exposes stable diagnostic state names") {
  thump_demo::CaretakerMachine caretaker;
  CHECK(caretaker.state_name() == "waiting_for_mouse_contact");
  REQUIRE(caretaker.contact_mouse());
  CHECK(caretaker.state_name() == "hunt_assigned");
  REQUIRE(caretaker.observe_mouse_tail());
  CHECK(caretaker.state_name() == "waiting_for_tail");
  REQUIRE(caretaker.hand_in_tail());
  CHECK(caretaker.state_name() == "settled");
}
