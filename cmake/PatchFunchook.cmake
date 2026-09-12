file(READ "${SOURCE_DIR}/cmake/capstone.cmake.in" _content)

# Insert PATCH_COMMAND before CONFIGURE_COMMAND in the ExternalProject_Add.
# Bracket args prevent cmake from substituting ${...} while writing this file.
if (NOT _content MATCHES "CAPSTONE_FIX_SCRIPT")
    string(REPLACE
        "    CONFIGURE_COMMAND \"\""
        [=[    PATCH_COMMAND "${CMAKE_COMMAND}" -DDIR=${CMAKE_CURRENT_BINARY_DIR}/capstone-src -P "${CAPSTONE_FIX_SCRIPT}"
    CONFIGURE_COMMAND ""]=]
        _content "${_content}")
    file(WRITE "${SOURCE_DIR}/cmake/capstone.cmake.in" "${_content}")
endif ()

file(READ "${SOURCE_DIR}/src/funchook.c" _content)
if (NOT _content MATCHES "commit_code_patch")
    string(REPLACE "#include \"funchook_internal.h\""
        "#include \"funchook_internal.h\"\n#ifdef __APPLE__\n#include \"code_patch_macos.hpp\"\n#endif"
        _content "${_content}")

    foreach(_operation install uninstall)
        if (_operation STREQUAL "install")
            set(_expected old_code)
            set(_replacement new_code)
        else ()
            set(_expected new_code)
            set(_replacement old_code)
        endif ()
        set(_original "            mem_state_t mstate;
            int rv = funchook_unprotect_begin(funchook, &mstate, entry->target_func, JUMP32_BYTE_SIZE);

            if (rv != 0) {
                return rv;
            }
            memcpy(entry->target_func, entry->${_replacement}, JUMP32_BYTE_SIZE);
            rv = funchook_unprotect_end(funchook, &mstate);
            if (rv != 0) {
                return rv;
            }
            flush_instruction_cache(entry->target_func, JUMP32_BYTE_SIZE);")
        string(FIND "${_content}" "${_original}" _position)
        if (_position EQUAL -1)
            message(FATAL_ERROR "Funchook ${_operation} patch site changed")
        endif ()
        string(REPLACE "${_original}" "#ifdef __APPLE__
            int rv = commit_code_patch(entry->target_func, entry->${_expected},
                                            entry->${_replacement}, JUMP32_BYTE_SIZE);
            if (rv != 0) {
                funchook_set_error_message(funchook, \"Code patch commit failed (Mach error %d)\", rv);
                return FUNCHOOK_ERROR_MEMORY_FUNCTION;
            }
#else
${_original}
#endif" _content "${_content}")
    endforeach ()
    file(WRITE "${SOURCE_DIR}/src/funchook.c" "${_content}")
endif ()
