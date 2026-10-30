/* SPDX-License-Identifier: MIT */
/* RinOS ptrace interface used by seccomp TRACE filters. */
#ifndef _SYS_PTRACE_H
#define _SYS_PTRACE_H

#include "types.h"
#include "syscall.h"
#include "../stdint.h"
#include <rin/ptrace_abi.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PTRACE_TRACEME       RIN_PTRACE_TRACEME
#define PTRACE_PEEKDATA      RIN_PTRACE_PEEKDATA
#define PTRACE_POKEDATA      RIN_PTRACE_POKEDATA
#define PTRACE_CONT          RIN_PTRACE_CONT
#define PTRACE_SINGLESTEP    RIN_PTRACE_SINGLESTEP
#define PTRACE_SYSCALL       RIN_PTRACE_SYSCALL
#define PTRACE_ATTACH        RIN_PTRACE_ATTACH
#define PTRACE_DETACH        RIN_PTRACE_DETACH
#define PTRACE_GETREGSET     RIN_PTRACE_GETREGSET
#define PTRACE_SETREGSET     RIN_PTRACE_SETREGSET
#define PTRACE_SEIZE         RIN_PTRACE_SEIZE
#define PTRACE_INTERRUPT     RIN_PTRACE_INTERRUPT
#define PTRACE_SETOPTIONS    RIN_PTRACE_SETOPTIONS
#define PTRACE_GETEVENTMSG   RIN_PTRACE_GETEVENTMSG
#define PTRACE_GETSIGINFO    RIN_PTRACE_GETSIGINFO
#define PTRACE_READ_MEMORY_V1  RIN_PTRACE_READ_MEMORY_V1
#define PTRACE_WRITE_MEMORY_V1 RIN_PTRACE_WRITE_MEMORY_V1

#define PTRACE_EVENT_SECCOMP RIN_PTRACE_EVENT_SECCOMP
#define PTRACE_O_TRACESECCOMP RIN_PTRACE_O_TRACESECCOMP
#define PTRACE_O_TRACEFORK   RIN_PTRACE_O_TRACEFORK
#define PTRACE_O_TRACEVFORK  RIN_PTRACE_O_TRACEVFORK
#define PTRACE_O_TRACECLONE  RIN_PTRACE_O_TRACECLONE
#define PTRACE_O_TRACEEXEC   RIN_PTRACE_O_TRACEEXEC
#define PTRACE_O_TRACEEXIT   RIN_PTRACE_O_TRACEEXIT

#define PTRACE_EVENT_FORK          RIN_PTRACE_EVENT_FORK
#define PTRACE_EVENT_VFORK         RIN_PTRACE_EVENT_VFORK
#define PTRACE_EVENT_CLONE         RIN_PTRACE_EVENT_CLONE
#define PTRACE_EVENT_EXEC          RIN_PTRACE_EVENT_EXEC
#define PTRACE_EVENT_VFORK_DONE    RIN_PTRACE_EVENT_VFORK_DONE
#define PTRACE_EVENT_EXIT          RIN_PTRACE_EVENT_EXIT
#define PTRACE_EVENT_SYSCALL_ENTRY RIN_PTRACE_EVENT_SYSCALL_ENTRY
#define PTRACE_EVENT_SYSCALL_EXIT  RIN_PTRACE_EVENT_SYSCALL_EXIT
#define PTRACE_EVENT_SINGLESTEP    RIN_PTRACE_EVENT_SINGLESTEP

typedef RinPtraceRegistersV1 rinos_ptrace_registers_v1;
typedef RinPtraceRegSetV1 rinos_ptrace_regset_v1;
typedef RinPtraceMemoryV1 rinos_ptrace_memory_v1;

static inline long ptrace(int request, pid_t pid, void* address, void* data) {
    return syscall(SYS_PTRACE, (uintptr_t)(unsigned int)request,
                   (uintptr_t)(intptr_t)pid, (uintptr_t)address,
                   (uintptr_t)data);
}

#ifdef __cplusplus
}
#endif

#endif /* _SYS_PTRACE_H */
