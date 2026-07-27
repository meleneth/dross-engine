#pragma once

#include <cstdint>
#include <vector>

namespace dross {

enum class MachineFamily : std::uint8_t {
  world_lifecycle,
  simulation_mode,
};

enum class MachineStateId : std::uint8_t {
  world_empty,
  world_loading,
  world_ready,
  world_running,
  world_saving,
  world_unloading,
  world_faulted,
  exploration,
  combat_pending,
  combat,
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
  combat_requested,
  safe_boundary_reached,
  combat_ended,
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

} // namespace dross
