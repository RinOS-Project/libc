/* SPDX-License-Identifier: MIT */

#include "stdlib.h"
#include "unistd.h"

#if defined(RIN_USERSPACE) && RIN_USERSPACE
char* realpath(const char* path, char* resolved_path) {
    return _rin_realpath_inline(path, resolved_path);
}
#endif
