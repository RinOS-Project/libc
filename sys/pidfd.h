/* SPDX-License-Identifier: MIT */
#ifndef _SYS_PIDFD_H
#define _SYS_PIDFD_H

#include "../syscall.h"
#include "types.h"
#include "../signal.h"

#ifdef __cplusplus
extern "C" {
#endif

/* pidfd_open always creates a close-on-exec descriptor. */
#ifndef PIDFD_NONBLOCK
#define PIDFD_NONBLOCK 0x800
#endif

static inline int pidfd_open(pid_t pid, unsigned int flags)
{
    return (int)syscall(SYS_PIDFD_OPEN, (int)pid, flags);
}

static inline int pidfd_send_signal(int pidfd, int signal_number,
                                    const siginfo_t* info,
                                    unsigned int flags)
{
    return (int)syscall(SYS_PIDFD_SEND_SIGNAL, pidfd, signal_number, info,
                        flags);
}

#ifdef __cplusplus
}
#endif

#endif
