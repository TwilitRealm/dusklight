#include "lua_helpers.hpp"

#include <cmath>

namespace luau_runtime::lua_helpers {

bool get_optional_bool(lua_State* state, int table, const char* field, bool fallback) {
    lua_getfield(state, table, field);
    const bool value = lua_isnil(state, -1) ? fallback : luaL_checkboolean(state, -1) != 0;
    lua_pop(state, 1);
    return value;
}

bool to_int64(lua_State* state, int index, int64_t& outValue) {
    if (lua_isinteger64(state, index)) {
        outValue = lua_tointeger64(state, index, nullptr);
        return true;
    }
    if (!lua_isnumber(state, index)) {
        return false;
    }
    const double value = lua_tonumber(state, index);
    constexpr double kMaxSafeInteger = 9007199254740991.0;
    if (!std::isfinite(value) || value < -kMaxSafeInteger || value > kMaxSafeInteger ||
        std::trunc(value) != value)
    {
        return false;
    }
    outValue = static_cast<int64_t>(value);
    return true;
}

int64_t check_int64(lua_State* state, int index) {
    int64_t value = 0;
    if (!to_int64(state, index, value)) {
        luaL_argerror(state, index, "integer value expected");
    }
    return value;
}

int64_t get_optional_int(lua_State* state, int table, const char* field, int64_t fallback) {
    lua_getfield(state, table, field);
    const int64_t value = lua_isnil(state, -1) ? fallback : check_int64(state, -1);
    lua_pop(state, 1);
    return value;
}

double get_optional_number(lua_State* state, int table, const char* field, double fallback) {
    lua_getfield(state, table, field);
    const double value = lua_isnil(state, -1) ? fallback : luaL_checknumber(state, -1);
    lua_pop(state, 1);
    return value;
}

double get_number(lua_State* state, int table, const char* field) {
    lua_getfield(state, table, field);
    const double value = luaL_checknumber(state, -1);
    lua_pop(state, 1);
    return value;
}

uint32_t get_optional_uint32(lua_State* state, int table, const char* field, uint32_t fallback) {
    lua_getfield(state, table, field);
    const uint32_t value = lua_isnil(state, -1) ? fallback : luaL_checkunsigned(state, -1);
    lua_pop(state, 1);
    return value;
}

uint32_t get_uint32(lua_State* state, int table, const char* field) {
    lua_getfield(state, table, field);
    const uint32_t value = luaL_checkunsigned(state, -1);
    lua_pop(state, 1);
    return value;
}

int32_t get_optional_int32(lua_State* state, int table, const char* field, int32_t fallback) {
    lua_getfield(state, table, field);
    const int32_t value = lua_isnil(state, -1) ? fallback : luaL_checkinteger(state, -1);
    lua_pop(state, 1);
    return value;
}

std::string get_optional_string(
    lua_State* state, int table, const char* field, std::string_view fallback) {
    std::string result;
    lua_getfield(state, table, field);
    if (!lua_isnil(state, -1)) {
        size_t length = 0;
        const char* value = luaL_checklstring(state, -1, &length);
        result.assign(value, length);
    } else {
        result = fallback;
    }
    lua_pop(state, 1);
    return result;
}

int ref_optional_function(lua_State* state, int table, const char* field) {
    lua_getfield(state, table, field);
    if (lua_isnil(state, -1)) {
        lua_pop(state, 1);
        return LUA_NOREF;
    }
    luaL_argexpected(state, lua_isfunction(state, -1), table, "function field");
    const int ref = lua_ref(state, -1);
    lua_pop(state, 1);
    return ref;
}

int ref_required_function(lua_State* state, int table, const char* field) {
    const int ref = ref_optional_function(state, table, field);
    if (ref == LUA_NOREF) {
        luaL_error(state, "field '%s' is required", field);
    }
    return ref;
}

}
