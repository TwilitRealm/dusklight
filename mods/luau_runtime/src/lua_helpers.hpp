#pragma once

#include "lua.h"
#include "lualib.h"

#include <string>
#include <typeinfo>

/*
 * Helper functions for API bound between Lua <-> C++
 */

namespace luau_runtime::lua_helpers {

bool get_optional_bool(lua_State* state, int table, const char* field, bool fallback);
double get_optional_number(lua_State* state, int table, const char* field, double fallback);
int32_t get_optional_int32(lua_State* state, int table, const char* field, int32_t fallback);
uint32_t get_optional_uint32(lua_State* state, int table, const char* field, uint32_t fallback);
std::string get_optional_string(
    lua_State* state, int table, const char* field, std::string_view fallback = {});

bool to_int64(lua_State* state, int index, int64_t& outValue);
int64_t check_int64(lua_State* state, int index);
int64_t get_optional_int(lua_State* state, int table, const char* field, int64_t fallback);
int ref_optional_function(lua_State* state, int table, const char* field);
int ref_required_function(lua_State* state, int table, const char* field);

double get_number(lua_State* state, int table, const char* field);
uint32_t get_uint32(lua_State* state, int table, const char* field);

/**
 * Concept that demands things be an enum. C++ is such a cool language.
 */
template<typename T>
concept Enum = std::is_enum_v<T>;

/**
 * Specifies a name/value pair for an enum. This is used by helpers to do automatic conversions
 * between the native and (string) Lua form.
 * @tparam T The enum type
 */
template <Enum T>
struct EnumName {
    T value;
    std::string_view name;

    constexpr EnumName(T value, std::string_view name) : value(value), name(name) {
    }
};

/**
 * Convert an enum's string name to the actual C++ value.
 *
 * Raises a Lua error if the string is not mapped.
 *
 * @tparam T Type of the enum
 * @param state The Lua state.
 * @param str The string value, from Lua.
 * @param options The predefined set of possible enum names.
 */
template <Enum T, size_t N>
T enum_str_to_value(lua_State* state, char const* str, EnumName<T> const(& options)[N]) {
    std::string_view strv(str);
    for (size_t i = 0; i < N; ++i) {
        if (options[i].name == strv) {
            return static_cast<T>(i);
        }
    }

    luaL_errorL(state, "Invalid enum value for %s: '%s'", typeid(T).name(), str);
}

/**
 * Convert an enum's C++ value to the Lua-exposed string.
 *
 * Raises a Lua error if the string is not mapped.
 *
 * @tparam T Type of the enum
 * @param state The Lua state.
 * @param value The C++ enum value.
 * @param options The predefined set of possible enum names.
 */
template <Enum T, size_t N>
std::string_view enum_value_to_str(lua_State* state, T value, EnumName<T> const(& options)[N]) {
    for (auto [nameValue, name] : options) {
        if (nameValue == value) {
            return name;
        }
    }

    luaL_errorL(state, "Attempted to return invalid enum to Lua!");
}

}
