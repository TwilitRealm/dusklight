#pragma once

#include <expected>

#include "api.h"

namespace mods {

/**
 * @c std::expected alias for @c ModResult
 */
template <typename T>
using ModExpected = std::expected<T, ModResult>;

}  // namespace mods
