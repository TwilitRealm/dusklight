#pragma once

#include <string_view>
#include "../runtime.hpp"

#include "lua.h"

namespace luau_runtime::services {

int open_audio_res(lua_State* state);
int open_log(lua_State* state);
int open_host(lua_State* state);
int open_resource(lua_State* state);
int open_overlay(lua_State* state);
int open_texture(lua_State* state);
int open_config(lua_State* state);
int open_ui(lua_State* state);

ModuleOpenFn module_factory(std::string_view name);

}  // namespace luau_runtime::services
