#pragma once

#include <tl/expected.hpp>

namespace dross {

template <class Value, class Error> using Result = tl::expected<Value, Error>;

} // namespace dross
