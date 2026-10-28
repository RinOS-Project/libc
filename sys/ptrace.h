/* SPDX-License-Identifier: MIT */
/* RinOS ptrace interface used by seccomp TRACE filters. */
#ifndef _SYS_PTRACE_H
#define _SYS_PTRACE_H

#include "types.h"
#include "syscall.h"
#include "../stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PTRACE_TRACEME       0
#define PTRACE_CONT          7
#define PTRACE_DETACH        17
#define PTRACE_SETOPTIONS    0x4200
#define PTRACE_GETEVENTMSG   0x4201

#define PTRACE_EVENT_SECCOMP 7
#define PTRACE_O_TRACESECCOMP 0x00000080UL

static inline long ptrace(int request, pid_t pid, void* address, void* data) {
    return syscall(SYS_PTRACE, (uintptr_t)(unsigned int)request,
                   (uintptr_t)(intptr_t)pid, (uintptr_t)address,
                   (uintptr_t)data);
}

#ifdef __cplusplus
}
#endif

#endif /* _SYS_PTRACE_H */
