#pragma once

#include "lua.h"
#include "mods/svc/config.h"

namespace luau_runtime::services {

void push_config_value(lua_State* state, const ConfigVarValue& value);

}
