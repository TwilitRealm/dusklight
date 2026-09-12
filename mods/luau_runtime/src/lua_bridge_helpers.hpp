#pragma once

// clang-format off
#include "lua.h"
#include "lualib.h"
#include "LuaBridge/LuaBridge.h"
// clang-format on
#include "runtime.hpp"

/**
 * Defines (i.e. implements) a LuaBridge3 Stack<T> converter for an enum that is passed by string
 * name.
 *
 * @param type The type name of the enum to convert.
 * @param names The constant field containing the string name <-> enum value mapping.
 * @see DECLARE_STRING_ENUM
 */
#define DEFINE_STRING_ENUM(type, names)                                                            \
    Result Stack<type>::push(lua_State* L, type value) {                                           \
        return Stack<std::string_view>::push(                                                      \
            L, ::luau_runtime::enum_value_to_str(L, value, names));                                \
    }                                                                                              \
                                                                                                   \
    TypeResult<type> Stack<type>::get(lua_State* L, int index) {                                   \
        const char* str = lua_tolstring(L, index, nullptr);                                        \
        return ::luau_runtime::enum_str_to_value(L, str, names);                                   \
    }

/**
 * Declares a LuaBridge3 Stack<T> converter for an enum that is passed by string name.
 *
 * @param type The type name of the enum to convert.
 * @see DEFINE_STRING_ENUM
 */
#define DECLARE_STRING_ENUM(type)                                                                  \
    template <>                                                                                    \
    struct Stack<type> {                                                                           \
        [[nodiscard]] static Result push(lua_State* L, type value);                                \
        [[nodiscard]] static TypeResult<type> get(lua_State* L, int index);                        \
    }

/**
 * Template specialization to allow luau_runtime::Vm& to be directly acquired
 * from a LuaBridge3 function.
 */
template <>
struct luabridge::Stack<luau_runtime::Vm&> {
    [[nodiscard]] static TypeResult<std::reference_wrapper<luau_runtime::Vm>> get(
        lua_State* L, int);
};

/**
 * Template specialization to allow luau_runtime::Vm const& to be directly acquired
 * from a LuaBridge3 function.
 */
template <>
struct luabridge::Stack<luau_runtime::Vm const&> {
    [[nodiscard]] static TypeResult<std::reference_wrapper<luau_runtime::Vm const>> get(
        lua_State* L, int);
};

namespace luau_runtime {

/**
 * Base type for "handle" types using LuaBridge3.
 *
 * Inherit from it, then register your derived class with LuaBridge3.
 * Provide the base class @ref unregister member function.
 */
class BridgeScriptHandle {
protected:
    uint64_t handle;
    explicit BridgeScriptHandle(uint64_t handle) noexcept;
    virtual void unregister_impl(lua_State* state, Vm& vm) = 0;

public:
    virtual ~BridgeScriptHandle();
    void unregister(lua_State* state, Vm& vm);
};

}  // namespace luau_runtime

/**
 * Convert a @ref luaBridge::LuaRef to an *optional* value.
 *
 * This uses the default LuaBridge3 handling for std::optional,
 * but just makes the type specification and casting a bit less verbose.
 *
 * @tparam T The type contained in the resulting optional.
 * @tparam TLuaRef The type of the lua ref. Let this be inferred, as table items are unnameable.
 * @param value Lua ref to convert to the desired type.
 */
template <typename T, typename TLuaRef>
std::optional<T> opt(TLuaRef const& value) {
    return value.template cast<std::optional<T>>().value();
}

/**
 * Convert a @ref luaBridge::LuaRef to a value, with a default fallback if nil.
 *
 * This uses the default LuaBridge3 handling for std::optional,
 * but just makes the type specification and casting a bit less verbose.
 *
 * @tparam T The resulting type.
 * @tparam TLuaRef The type of the lua ref. Let this be inferred, as table items are unnameable.
 * @param value Lua ref to convert to the desired type.
 * @param default_value The default value if the lua item is nil.
 */
template <typename T, typename TLuaRef>
T opt_or(TLuaRef const& value, T const& default_value) {
    return opt<T>(value).value_or(default_value);
}
