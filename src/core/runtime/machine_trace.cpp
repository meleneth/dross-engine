#include <dross/runtime/machine_trace.hpp>

namespace dross {

void InMemoryMachineTrace::record(const MachineTraceEntry entry) { entries_.push_back(entry); }

const std::vector<MachineTraceEntry>& InMemoryMachineTrace::entries() const noexcept {
  return entries_;
}

} // namespace dross
