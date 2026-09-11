#pragma once

#include "../runtime.hpp"
#include "lua.h"

namespace luau_runtime::services {

void push_ui_handle(lua_State* state, Vm& vm, uint64_t value, HandleKind kind);

}
