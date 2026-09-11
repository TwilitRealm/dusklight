cmake_minimum_required(VERSION 3.25)
project(luau_runtime CXX)

if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(DUSK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../.." CACHE PATH "Path to dusk source root")
    option(DUSK_MOD_USE_FULL_TREE "Use full build instead of the minimal mod SDK" OFF)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    if (DUSK_MOD_USE_FULL_TREE)
        add_subdirectory("${DUSK_DIR}" dusk EXCLUDE_FROM_ALL)
    else ()
        add_subdirectory("${DUSK_DIR}/sdk" dusk-sdk EXCLUDE_FROM_ALL)
    endif ()
endif ()

include(FetchContent)

set(LUAU_BUILD_CLI OFF CACHE BOOL "" FORCE)
set(LUAU_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LUAU_BUILD_WEB OFF CACHE BOOL "" FORCE)
set(LUAU_WERROR OFF CACHE BOOL "" FORCE)
FetchContent_Declare(luau
    URL https://github.com/luau-lang/luau/archive/refs/tags/0.734.tar.gz
    URL_HASH SHA256=cb55a891226d8c70284e22eb9281cc2b4496c709a4050f52aaa18a355fe7b1a3
    DOWNLOAD_EXTRACT_TIMESTAMP FALSE
    EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(luau)

FetchContent_Declare(luabridge3
    URL https://github.com/TwilitRealm/LuaBridge3/archive/refs/tags/twilit-v1.zip
    URL_HASH SHA256=aa65e6e3446363d03842134182db28e6206e08d366b8efb8f66d26f96a3314be
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(luabridge3)

add_mod(luau_runtime
    SOURCES
        src/services/bindings.cpp
        src/services/config.cpp
        src/services/config.hpp
        src/services/services.cpp
        src/services/services.hpp
        src/services/ui.cpp
        src/services/ui.hpp
        src/lua_bridge_helpers.cpp
        src/lua_bridge_helpers.hpp
        src/lua_helpers.cpp
        src/lua_helpers.hpp
        src/runtime.hpp
        src/runtime.cpp
    MOD_JSON mod.json
    RES_DIR res
    BUNDLE
)

target_precompile_headers(luau_runtime PRIVATE src/lua_bridge_helpers.hpp src/lua_helpers.hpp)

# As far as I can tell, Luau already does stack checks.
target_compile_definitions(luau_runtime PRIVATE LUABRIDGE_SAFE_STACK_CHECKS=0)
add_subdirectory(src/services/audio_res)

target_link_libraries(luau_runtime PRIVATE Luau.VM Luau.Compiler LuaBridge)
