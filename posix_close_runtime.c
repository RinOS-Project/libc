/* SPDX-License-Identifier: MIT */
#define RIN_POSIX_CLOSE_RUNTIME_IMPLEMENTATION 1
#include "unistd.h"

int posix_close(int fd, int flags) {
    return _rin_posix_close_impl(fd, flags);
}
