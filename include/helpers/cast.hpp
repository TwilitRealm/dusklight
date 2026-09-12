#pragma once

#include <limits>
#include <span>
#include <utility>

/**
 * Helper functions for performing casts.
 */
namespace dusk::helpers::cast {
/**
 * Implementation details of dusk::helpers::cast.
 */
namespace _impl {
[[noreturn]] void overrun_high();
[[noreturn]] void overrun_low();
}  // namespace _impl

template <typename T>
concept IntCastable = std::is_integral_v<T> && std::is_trivially_copyable_v<T>;

/**
 * Helper type that allows easily casting between integer types,
 * that safely aborts if an overflow were to occur.
 * Usage:
 * @code
 * size_t foobar = 20;
 * int real = bounded_cast(foobar);
 * @endcode
 */
template <IntCastable Source>
struct bounded_cast {
    Source src;

    template <IntCastable Target>
    [[nodiscard]] constexpr operator Target() const {
        if (std::cmp_greater(src, std::numeric_limits<Target>::max())) [[unlikely]] {
            _impl::overrun_high();
        }

        if (std::cmp_less(src, std::numeric_limits<Target>::min())) [[unlikely]] {
            _impl::overrun_low();
        }

        return static_cast<Target>(src);
    }
};
}  // namespace dusk::helpers::cast
