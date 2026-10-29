/* SPDX-License-Identifier: MIT */
/* Export the XSI lockf symbol using the header's fcntl-backed implementation. */

#define lockf __rin_lockf_inline
#include "unistd.h"
#undef lockf

int lockf(int fd, int function, off_t size) {
    return __rin_lockf_inline(fd, function, size);
}
