#pragma once

#include <string>

#include <mods/expected.hpp>
#include <mods/svc/config.h>
#include <mods/svc/log.hpp>

/*
 * C++ wrapper for svc_config
 */

namespace mods::config {

/**
 * Valid value types for config vars.
 */
template <typename T>
concept ValueType = std::is_same_v<T, bool> || std::is_same_v<T, int64_t> ||
                    std::is_same_v<T, double> || std::is_same_v<T, std::string>;

/**
 * RAII and type-safe wrapper for a registered config var. Obtained from @c register_var
 */
template <ValueType T>
struct Handle final {
    ConfigVarHandle handle{};

    Handle() = default;
    explicit Handle(ConfigVarHandle const handle) : handle(handle) {}

    Handle(Handle const&) = delete;
    Handle(Handle&& other) noexcept { *this = std::move(other); }

    Handle& operator=(Handle&& other) noexcept {
        unregister(handle);

        handle = other.handle;
        other.handle = 0;
        return *this;
    }

    ~Handle() { unregister(handle); }

    ModExpected<T> get() const {
        if (!svc_config) {
            return std::unexpected(MOD_UNAVAILABLE);
        }

        T out{};
        auto const result = get_impl(out);
        if (result == MOD_OK) {
            return out;
        }

        return std::unexpected(result);
    }
    ModExpected<void> set(T const& value) {
        if (!svc_config) {
            return std::unexpected(MOD_UNAVAILABLE);
        }

        auto const result = set_impl(value);
        if (result == MOD_OK) {
            return {};
        }

        return std::unexpected(result);
    }

private:
    static void unregister(ConfigVarHandle handle) {
        if (handle) {
            const auto result = svc_config->unregister_var(mod_ctx, handle);
            if (result != MOD_OK) {
                // We can't throw in destructors so...
                log::error("Failed to unregister var!");
            }
        }
    }

    ModResult get_impl(T& out) const;
    ModResult set_impl(T const& in);
};

/**
 * Register a config var with a given name and default value.
 */
template <ValueType T>
ModExpected<Handle<T>> register_var(std::string const& name, T const& defaultValue);

namespace detail {

template <ValueType T>
void apply_default_to_desc(ConfigVarDesc& desc, T const& value);

template <>
inline void apply_default_to_desc<int64_t>(ConfigVarDesc& desc, int64_t const& value) {
    desc.type = CONFIG_VAR_INT;
    desc.default_int = value;
}

template <>
inline void apply_default_to_desc<bool>(ConfigVarDesc& desc, bool const& value) {
    desc.type = CONFIG_VAR_BOOL;
    desc.default_bool = value;
}

template <>
inline void apply_default_to_desc<double>(ConfigVarDesc& desc, double const& value) {
    desc.type = CONFIG_VAR_FLOAT;
    desc.default_float = value;
}

template <>
inline void apply_default_to_desc<std::string>(ConfigVarDesc& desc, std::string const& value) {
    desc.type = CONFIG_VAR_STRING;
    desc.default_string = value.c_str();
}

}  // namespace detail

template <ValueType T>
ModExpected<Handle<T>> register_var(std::string const& name, T const& defaultValue) {
    if (!svc_config) {
        return std::unexpected(MOD_UNAVAILABLE);
    }

    ConfigVarDesc rawDesc CONFIG_VAR_DESC_INIT;
    rawDesc.name = name.c_str();
    detail::apply_default_to_desc(rawDesc, defaultValue);

    ConfigVarHandle handle;
    auto const result = svc_config->register_var(mod_ctx, &rawDesc, &handle);
    if (result != MOD_OK) {
        return std::unexpected(result);
    }

    return Handle<T>(handle);
}

inline ModExpected<Handle<int64_t>> register_var(std::string const& name, int const& defaultValue) {
    return register_var(name, static_cast<int64_t>(defaultValue));
}

template <>
inline ModResult Handle<int64_t>::get_impl(int64_t& out) const {
    return svc_config->get_int(mod_ctx, handle, &out);
}

template <>
inline ModResult Handle<bool>::get_impl(bool& out) const {
    return svc_config->get_bool(mod_ctx, handle, &out);
}

template <>
inline ModResult Handle<double>::get_impl(double& out) const {
    return svc_config->get_float(mod_ctx, handle, &out);
}

template <>
inline ModResult Handle<std::string>::get_impl(std::string& out) const {
    size_t length = 0;
    auto result = svc_config->get_string(mod_ctx, handle, nullptr, 0, &length);
    if (result != MOD_OK) {
        return result;
    }

    out.resize(length);
    result = svc_config->get_string(mod_ctx, handle, out.data(), out.length() + 1, nullptr);
    if (result != MOD_OK) {
        return result;
    }

    return MOD_OK;
}

template <>
inline ModResult Handle<int64_t>::set_impl(int64_t const& in) {
    return svc_config->set_int(mod_ctx, handle, in);
}

template <>
inline ModResult Handle<bool>::set_impl(bool const& in) {
    return svc_config->set_bool(mod_ctx, handle, in);
}

template <>
inline ModResult Handle<double>::set_impl(double const& in) {
    return svc_config->set_float(mod_ctx, handle, in);
}

template <>
inline ModResult Handle<std::string>::set_impl(std::string const& in) {
    return svc_config->set_string(mod_ctx, handle, in.c_str());
}

}  // namespace mods::config
