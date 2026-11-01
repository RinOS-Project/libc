/* SPDX-License-Identifier: MIT */
/* Linux openat2 UAPI, kept source-compatible with linux/openat2.h. */
#ifndef RINOS_LIBC_LINUX_OPENAT2_H
#define RINOS_LIBC_LINUX_OPENAT2_H

#include <stdint.h>

struct open_how {
    uint64_t flags;
    uint64_t mode;
    uint64_t resolve;
};

#define RESOLVE_NO_XDEV       UINT64_C(0x01)
#define RESOLVE_NO_MAGICLINKS UINT64_C(0x02)
#define RESOLVE_NO_SYMLINKS   UINT64_C(0x04)
#define RESOLVE_BENEATH       UINT64_C(0x08)
#define RESOLVE_IN_ROOT       UINT64_C(0x10)
#define RESOLVE_CACHED        UINT64_C(0x20)

#endif /* RINOS_LIBC_LINUX_OPENAT2_H */
