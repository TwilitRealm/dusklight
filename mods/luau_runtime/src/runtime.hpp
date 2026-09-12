#pragma once

#include "lua.h"
#include "lualib.h"

#include "mods/api.h"
#include "mods/svc/config.h"
#include "mods/svc/host.h"
#include "mods/svc/log.h"
#include "mods/svc/overlay.h"
#include "mods/svc/resource.h"
#include "mods/svc/texture.h"
#include "mods/svc/ui.h"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace luau_runtime {

constexpr size_t kMemoryLimit = 64u * 1024u * 1024u;
constexpr auto kUpdateBudget = std::chrono::milliseconds{250};
constexpr auto kLifecycleBudget = std::chrono::seconds{5};
constexpr auto kCallbackBudget = std::chrono::milliseconds{250};

struct MemoryBudget {
    size_t used = 0;
    size_t limit = kMemoryLimit;
};

struct Vm;

struct Callback {
    Vm* vm = nullptr;
    std::array<int, 8> refs{};
    std::string returnedString;
    int tag = 0;
};

struct Vm {
    MemoryBudget memory;
    lua_State* state = nullptr;
    ModContext* subject = nullptr;
    std::vector<int> updateRefs;
    std::vector<int> shutdownRefs;
    std::unordered_map<std::string, int> moduleRefs;
    std::unordered_set<std::string> loadingModules;
    std::vector<std::unique_ptr<Callback>> callbacks;
    std::chrono::steady_clock::time_point deadline{};
    unsigned callDepth = 0;
    bool deadlineActive = false;

    Vm() = default;
    Vm(Vm const&) = delete;
    Vm(Vm&&) = delete;
    ~Vm();
};

enum class HandleKind : uint8_t {
    ConfigVar,
    ConfigSubscription,
    Overlay,
    Texture,
    UiWindow,
    UiDialog,
    UiElement,
    UiStyle,
    UiMenuTab,
    UiList,
};

struct ScriptHandle {
    Vm* vm = nullptr;
    uint64_t value = 0;
    HandleKind kind = HandleKind::ConfigVar;
    ConfigVarType configType = CONFIG_VAR_BOOL;
};

using ModuleOpenFn = int (*)(lua_State* state);

Vm& vm_from_upvalue(lua_State* state);
Vm& vm_from_registry(lua_State* state);
void push_vm_closure(lua_State* state, Vm& vm, lua_CFunction function, const char* name);
void set_function(lua_State* state, Vm& vm, const char* name, lua_CFunction function);

[[noreturn]] void service_unavailable(lua_State* state, const char* name);
void check_result(lua_State* state, ModResult result, const char* operation);

Callback& retain_callback(Vm& vm);
bool call_ref(Vm& vm, int ref, int argumentCount, int resultCount,
    std::chrono::steady_clock::duration budget, std::string& outError);
void fail_callback(Vm& vm, std::string_view callbackName, std::string_view error);
ModResult set_error(ModError* outError, ModResult result, std::string_view message);

void create_handle_metatable(
    lua_State* state, const char* name, const luaL_Reg* methods, const char* typeName);
ScriptHandle& check_handle(lua_State* state, int index, const char* metatable, HandleKind kind);
void push_handle(lua_State* state, Vm& vm, uint64_t value, HandleKind kind, const char* metatable,
    ConfigVarType configType = CONFIG_VAR_BOOL);

}  // namespace luau_runtime
