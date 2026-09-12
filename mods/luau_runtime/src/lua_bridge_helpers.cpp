#include "lua_bridge_helpers.hpp"

luabridge::TypeResult<std::reference_wrapper<luau_runtime::Vm>>
luabridge::Stack<luau_runtime::Vm&>::get(lua_State* L, int) {
    auto& ref = luau_runtime::vm_from_registry(L);
    return std::reference_wrapper(ref);
}

luabridge::TypeResult<std::reference_wrapper<const luau_runtime::Vm>>
luabridge::Stack<const luau_runtime::Vm&>::get(lua_State* L, int) {
    auto const& ref = luau_runtime::vm_from_registry(L);
    return std::reference_wrapper(ref);
}

namespace luau_runtime {

BridgeScriptHandle::BridgeScriptHandle(uint64_t const handle) noexcept : handle(handle) {}

BridgeScriptHandle::~BridgeScriptHandle() = default;

void BridgeScriptHandle::unregister(lua_State* state, Vm& vm) {
    if (handle == 0) {
        luaL_error(state, "Stale handle");
    }

    try {
        unregister_impl(state, vm);
    } catch (std::exception const&) {
        // If an exception occurs, disallow the Lua code from attempting to re-run unregister.
        // Since we assume the object may be in an undefined state (but hopefully not?)
        handle = 0;
        throw;
    }

    handle = 0;
}

}  // namespace luau_runtime
