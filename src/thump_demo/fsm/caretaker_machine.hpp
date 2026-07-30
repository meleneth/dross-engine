#pragma once

#include <cstdint>
#include <memory>
#include <string_view>

namespace thump_demo {

enum class CaretakerState : std::uint8_t {
  waiting_for_mouse_contact,
  hunt_assigned,
  waiting_for_tail,
  settled,
};

struct CaretakerSnapshot {
  CaretakerState state;

  [[nodiscard]] bool operator==(const CaretakerSnapshot&) const = default;
};

class CaretakerMachine {
public:
  CaretakerMachine();
  ~CaretakerMachine();
  CaretakerMachine(CaretakerMachine&&) noexcept;
  CaretakerMachine& operator=(CaretakerMachine&&) noexcept;
  CaretakerMachine(const CaretakerMachine&) = delete;
  CaretakerMachine& operator=(const CaretakerMachine&) = delete;

  [[nodiscard]] bool contact_mouse();
  [[nodiscard]] bool observe_mouse_tail();
  [[nodiscard]] bool hand_in_tail();

  [[nodiscard]] CaretakerState state() const;
  [[nodiscard]] std::string_view state_name() const;
  [[nodiscard]] CaretakerSnapshot snapshot() const;
  [[nodiscard]] bool restore(CaretakerSnapshot snapshot);

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace thump_demo
