/* SPDX-License-Identifier: MIT */
#ifndef _SYS_SYSINFO_H
#define _SYS_SYSINFO_H

#include "../errno.h"
#include "../limits.h"
#include "syscall.h"
#include <rin/system_info_snapshot_v1.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Linux-compatible public layout.  RinOS currently supplies uptime, RAM and
 * process count; unsupported load, swap, buffer and high-memory values are 0. */
struct sysinfo {
    long uptime;
    unsigned long loads[3];
    unsigned long totalram;
    unsigned long freeram;
    unsigned long sharedram;
    unsigned long bufferram;
    unsigned long totalswap;
    unsigned long freeswap;
    unsigned short procs;
    unsigned short pad;
    unsigned long totalhigh;
    unsigned long freehigh;
    unsigned int mem_unit;
#if defined(__SIZEOF_LONG__) && __SIZEOF_LONG__ == 4
    char _f[8];
#elif defined(__SIZEOF_LONG__) && __SIZEOF_LONG__ == 8
    char _f[4];
#else
#error "sysinfo ABI requires 32-bit or 64-bit unsigned long"
#endif
};

#if defined(__cplusplus)
static_assert(sizeof(struct sysinfo) ==
                  (sizeof(unsigned long) == 8 ? 112u : 64u),
              "struct sysinfo ABI drift");
#else
_Static_assert(sizeof(struct sysinfo) ==
                   (sizeof(unsigned long) == 8 ? 112u : 64u),
               "struct sysinfo ABI drift");
#endif

static inline int sysinfo(struct sysinfo* information) {
    if (information == 0) {
        errno = EFAULT;
        return -1;
    }
    return (int)syscall(RIN_SYS_SYSTEM_INFO_SNAPSHOT_V1, information,
                        RIN_SYSTEM_INFO_SNAPSHOT_LAYOUT_LINUX_SYSINFO);
}

#ifdef __cplusplus
}
#endif

#endif /* _SYS_SYSINFO_H */
