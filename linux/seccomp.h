/* SPDX-License-Identifier: MIT */
/* Seccomp mode and return-action ABI. */
#ifndef RIN_LIBC_LINUX_SECCOMP_H
#define RIN_LIBC_LINUX_SECCOMP_H

#include "filter.h"

#ifndef SECCOMP_MODE_DISABLED
#define SECCOMP_MODE_DISABLED 0
#define SECCOMP_MODE_STRICT   1
#define SECCOMP_MODE_FILTER   2
#endif

#define SECCOMP_SET_MODE_STRICT  0
#define SECCOMP_SET_MODE_FILTER  1
#define SECCOMP_GET_ACTION_AVAIL 2
#define SECCOMP_GET_NOTIF_SIZES  3

#define SECCOMP_FILTER_FLAG_NEW_LISTENER (1U << 3)
#define SECCOMP_USER_NOTIF_FLAG_CONTINUE (1U << 0)

#ifndef SECCOMP_RET_KILL_THREAD
#define SECCOMP_RET_KILL_THREAD  0x00000000u
#define SECCOMP_RET_KILL_PROCESS 0x80000000u
#define SECCOMP_RET_TRAP         0x00030000u
#define SECCOMP_RET_ERRNO        0x00050000u
#define SECCOMP_RET_USER_NOTIF   0x7fc00000u
#define SECCOMP_RET_TRACE        0x7ff00000u
#define SECCOMP_RET_LOG          0x7ffc0000u
#define SECCOMP_RET_ALLOW        0x7fff0000u
#define SECCOMP_RET_ACTION_FULL  0xffff0000u
#define SECCOMP_RET_DATA         0x0000ffffu
#endif

struct seccomp_data {
    int32_t nr;
    uint32_t arch;
    uint64_t instruction_pointer;
    uint64_t args[6];
};

struct seccomp_notif {
    uint64_t id;
    uint32_t pid;
    uint32_t flags;
    struct seccomp_data data;
};

struct seccomp_notif_resp {
    uint64_t id;
    int64_t val;
    int32_t error;
    uint32_t flags;
};

struct seccomp_notif_sizes {
    uint16_t seccomp_notif;
    uint16_t seccomp_notif_resp;
    uint16_t seccomp_data;
};

#define SECCOMP_IOCTL_NOTIF_RECV     0xc0502100u
#define SECCOMP_IOCTL_NOTIF_SEND     0xc0182101u
#define SECCOMP_IOCTL_NOTIF_ID_VALID 0x40082102u

#ifndef AUDIT_ARCH_I386
#define AUDIT_ARCH_I386    0x40000003u
#define AUDIT_ARCH_X86_64  0xc000003eu
#endif

#endif /* RIN_LIBC_LINUX_SECCOMP_H */
