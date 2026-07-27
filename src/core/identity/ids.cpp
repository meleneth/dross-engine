#include <dross/identity/ids.hpp>

#include <ostream>

namespace dross {

std::ostream& operator<<(std::ostream& output, const EntityId value) {
  return output << "entity:" << value.value();
}

std::ostream& operator<<(std::ostream& output, const WorldInstanceId value) {
  return output << "world:" << value.value();
}

std::ostream& operator<<(std::ostream& output, const CommandId value) {
  return output << "command:" << value.value();
}

std::ostream& operator<<(std::ostream& output, const CausationId value) {
  return output << "causation:" << value.value();
}

std::ostream& operator<<(std::ostream& output, const CorrelationId value) {
  return output << "correlation:" << value.value();
}

} // namespace dross
