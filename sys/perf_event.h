/* SPDX-License-Identifier: MIT */
#ifndef RIN_LIBC_SYS_PERF_EVENT_H
#define RIN_LIBC_SYS_PERF_EVENT_H

#include <errno.h>
#include <linux/perf_event.h>
#include <rin/linux_syscall_abi.h>
#include <sys/syscall.h>
#include <sys/types.h>

#if defined(__x86_64__) || defined(_M_X64) || defined(RIN_ARCH_64)
#define RIN_LINUX_NR_PERF_EVENT_OPEN RIN_LINUX_NR_PERF_EVENT_OPEN_X86_64
#elif defined(__i386__) || defined(_M_IX86) || defined(RIN_ARCH_32)
#define RIN_LINUX_NR_PERF_EVENT_OPEN RIN_LINUX_NR_PERF_EVENT_OPEN_I386
#else
#error "perf_event_open is currently available only on x86 targets"
#endif

static inline int perf_event_open(struct perf_event_attr* attr, pid_t pid,
                                  int cpu, int group_fd,
                                  unsigned long flags) {
    long result;
    if (attr == 0) {
        errno = EFAULT;
        return -1;
    }
    result = syscall(RIN_LINUX_NR_PERF_EVENT_OPEN, attr, pid, cpu, group_fd,
                     flags);
    if (result < 0 && result >= -4095) {
        errno = (int)-result;
        return -1;
    }
    return (int)result;
}

#endif /* RIN_LIBC_SYS_PERF_EVENT_H */
