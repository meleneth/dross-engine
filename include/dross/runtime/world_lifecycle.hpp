#pragma once

#include <dross/runtime/machine_trace.hpp>

#include <cstdint>
#include <memory>

namespace dross {

enum class WorldLifecycleState : std::uint8_t {
  empty,
  loading,
  ready,
  running,
  saving,
  unloading,
  faulted,
};

struct WorldLifecycleSnapshot {
  WorldLifecycleState state;

  [[nodiscard]] bool operator==(const WorldLifecycleSnapshot&) const = default;
};

class WorldLifecycle {
public:
  explicit WorldLifecycle(MachineTraceSink& trace);
  ~WorldLifecycle();
  WorldLifecycle(WorldLifecycle&&) noexcept;
  WorldLifecycle& operator=(WorldLifecycle&&) noexcept;
  WorldLifecycle(const WorldLifecycle&) = delete;
  WorldLifecycle& operator=(const WorldLifecycle&) = delete;

  [[nodiscard]] bool begin_load();
  [[nodiscard]] bool load_succeeded();
  [[nodiscard]] bool load_failed();
  [[nodiscard]] bool begin_run();
  [[nodiscard]] bool begin_save();
  [[nodiscard]] bool save_succeeded();
  [[nodiscard]] bool save_io_failed();
  [[nodiscard]] bool begin_unload();
  [[nodiscard]] bool unload_succeeded();
  [[nodiscard]] bool fatal_fault();
  [[nodiscard]] bool request_runtime_work();
  [[nodiscard]] bool request_save_boundary();

  [[nodiscard]] WorldLifecycleState state() const;
  [[nodiscard]] WorldLifecycleSnapshot snapshot() const;
  [[nodiscard]] bool restore(WorldLifecycleSnapshot snapshot);

private:
  struct Impl;
  template <class Event> [[nodiscard]] bool process(Event event, MachineEventId event_id);

  MachineTraceSink* trace_;
  std::unique_ptr<Impl> impl_;
};

} // namespace dross
