/* SPDX-License-Identifier: MIT */
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
