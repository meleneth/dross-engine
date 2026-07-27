#pragma once

#include <cstdint>
#include <memory>
#include <vector>

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

enum class MachineFamily : std::uint8_t {
  world_lifecycle,
};

enum class MachineStateId : std::uint8_t {
  world_empty,
  world_loading,
  world_ready,
  world_running,
  world_saving,
  world_unloading,
  world_faulted,
};

enum class MachineEventId : std::uint8_t {
  begin_load,
  load_succeeded,
  load_failed,
  begin_run,
  begin_save,
  save_succeeded,
  save_io_failed,
  begin_unload,
  unload_succeeded,
  fatal_fault,
  restore,
};

enum class MachineEventOutcome : std::uint8_t {
  transitioned,
  rejected,
};

struct MachineTraceEntry {
  MachineFamily machine;
  MachineStateId source;
  MachineStateId destination;
  MachineEventId event;
  MachineEventOutcome outcome;

  [[nodiscard]] bool operator==(const MachineTraceEntry&) const = default;
};

class MachineTraceSink {
public:
  virtual ~MachineTraceSink() = default;
  virtual void record(MachineTraceEntry entry) = 0;
};

class NullMachineTrace final : public MachineTraceSink {
public:
  void record(MachineTraceEntry) override {}
};

class InMemoryMachineTrace final : public MachineTraceSink {
public:
  void record(MachineTraceEntry entry) override;
  [[nodiscard]] const std::vector<MachineTraceEntry>& entries() const noexcept;

private:
  std::vector<MachineTraceEntry> entries_;
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
