#include <dross/hex/hex_topology.hpp>

#include <utility>

namespace dross {

Result<EdgeKey, EdgeKeyError> EdgeKey::between(HexCellId first, HexCellId second) {
  if (first == second) {
    return tl::unexpected{EdgeKeyError::identical_endpoints};
  }
  if (second < first) {
    std::swap(first, second);
  }
  return EdgeKey{std::move(first), std::move(second)};
}

} // namespace dross
