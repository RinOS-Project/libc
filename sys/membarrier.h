/* SPDX-License-Identifier: MIT */
#if defined(__STDC_HOSTED__) && __STDC_HOSTED__ && \
    (!defined(RIN_FREESTANDING) || !(RIN_FREESTANDING)) && \
    defined(__linux__) && (defined(__GNUC__) || defined(__clang__)) && \
    !defined(RIN_USERSPACE) && !defined(RIN_KERNEL) && \
    !defined(RIN_ARCH_32) && !defined(RIN_ARCH_64)
#include <sys/mman.h>
#include <linux/membarrier.h>
#else
#ifndef _SYS_MEMBARRIER_H
#define _SYS_MEMBARRIER_H

#include "../linux/membarrier.h"
#include "syscall.h"

#ifdef __cplusplus
extern "C" {
#endif

static inline int membarrier(int command, unsigned int flags, int cpu_id) {
    return (int)syscall(RIN_SYS_MEMBARRIER, command, flags, cpu_id);
}

#ifdef __cplusplus
}
#endif

#endif /* _SYS_MEMBARRIER_H */
#endif /* hosted Linux uses native mmap/membarrier provider */
