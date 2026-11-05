/* SPDX-License-Identifier: MIT */
#if defined(__STDC_HOSTED__) && __STDC_HOSTED__ && \
    (!defined(RIN_FREESTANDING) || !(RIN_FREESTANDING)) && \
    defined(__linux__) && (defined(__GNUC__) || defined(__clang__)) && \
    !defined(RIN_USERSPACE) && !defined(RIN_KERNEL) && \
    !defined(RIN_ARCH_32) && !defined(RIN_ARCH_64) && \
    !defined(_RIN_PIDFD_SEND_SIGNAL_SYSCALL4)
#if defined(__has_include_next)
#if __has_include_next(<sys/pidfd.h>)
#include_next <sys/pidfd.h>
#endif
#endif
/* If the host CRT has no pidfd header, do not expose RinOS syscall numbers. */
#else
#ifndef _SYS_PIDFD_H
#define _SYS_PIDFD_H

#include "../features.h"
#include "../syscall.h"
#include "types.h"
#include "../signal.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__RIN_FEATURES_GNU)
static inline int pidfd_getfd(int pidfd, int target_fd, unsigned int flags)
{
    return (int)syscall(SYS_PIDFD_GETFD, pidfd, target_fd, flags);
}

/* pidfd_open always creates a close-on-exec descriptor. */
#ifndef PIDFD_NONBLOCK
#define PIDFD_NONBLOCK 0x800
#endif
#ifndef PIDFD_THREAD
#define PIDFD_THREAD 0x80
#endif

static inline int pidfd_open(pid_t pid, unsigned int flags)
{
    return (int)syscall(SYS_PIDFD_OPEN, (int)pid, flags);
}
#endif

#ifndef _RIN_PIDFD_SEND_SIGNAL_SYSCALL4
#define _RIN_PIDFD_SEND_SIGNAL_SYSCALL4(number, argument1, argument2, argument3, argument4) \
    _syscall4((uintptr_t)(number), (uintptr_t)(argument1), \
              (uintptr_t)(argument2), (uintptr_t)(argument3), \
              (uintptr_t)(argument4))
#endif

static inline int __rin_pidfd_send_signal(int pidfd, int signal_number,
                                          const siginfo_t* info,
                                          unsigned int flags)
{
    return (int)__rin_syscall_posixize(_RIN_PIDFD_SEND_SIGNAL_SYSCALL4(
        SYS_PIDFD_SEND_SIGNAL, pidfd, signal_number, info, flags));
}

#if defined(__RIN_FEATURES_GNU)
static inline int pidfd_send_signal(int pidfd, int signal_number,
                                    const siginfo_t* info,
                                    unsigned int flags)
{
    return __rin_pidfd_send_signal(pidfd, signal_number, info, flags);
}
#endif

#ifdef __cplusplus
}
#endif

#endif
#endif /* hosted Linux delegates when available; otherwise exposes no RinOS ABI */
