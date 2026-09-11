#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int commit_code_patch(void* target, const void* expected, const void* replacement, size_t size);

#ifdef __cplusplus
}
#endif
