/* SPDX-License-Identifier: MIT */

#include "sys/epoll.h"

#include "limits.h"
#include "sys/syscall.h"

int epoll_create1(int flags)
{
    intptr_t result;
    if ((flags & ~EPOLL_CLOEXEC) != 0) {
        errno = EINVAL;
        return -1;
    }
    result = __rin_syscall_posixize(_syscall1(
        SYS_EPOLL_CREATE1, (uintptr_t)(unsigned int)flags));
    if (result < 0) return -1;
    if (result > INT_MAX) {
        errno = EOVERFLOW;
        return -1;
    }
    return (int)result;
}

int epoll_ctl(int epoll_fd, int operation, int fd,
              struct epoll_event* event)
{
    intptr_t result = __rin_syscall_posixize(_syscall4(
        SYS_EPOLL_CTL, (uintptr_t)(unsigned int)epoll_fd,
        (uintptr_t)(unsigned int)operation, (uintptr_t)(unsigned int)fd,
        (uintptr_t)event));
    return result < 0 ? -1 : (int)result;
}

int epoll_wait(int epoll_fd, struct epoll_event* events,
               int max_events, int timeout_ms)
{
    intptr_t result = __rin_syscall_posixize(_syscall4(
        SYS_EPOLL_WAIT, (uintptr_t)(unsigned int)epoll_fd,
        (uintptr_t)events, (uintptr_t)(unsigned int)max_events,
        (uintptr_t)(unsigned int)timeout_ms));
    return result < 0 ? -1 : (int)result;
}

int epoll_pwait(int epoll_fd, struct epoll_event* events,
                int max_events, int timeout_ms, const sigset_t* signal_mask)
{
    intptr_t result = __rin_syscall_posixize(_syscall6(
        SYS_EPOLL_PWAIT, (uintptr_t)(unsigned int)epoll_fd,
        (uintptr_t)events, (uintptr_t)(unsigned int)max_events,
        (uintptr_t)(unsigned int)timeout_ms, (uintptr_t)signal_mask,
        (uintptr_t)sizeof(sigset_t)));
    return result < 0 ? -1 : (int)result;
}

int epoll_pwait2(int epoll_fd, struct epoll_event* events,
                 int max_events, const struct timespec* timeout,
                 const sigset_t* signal_mask)
{
    intptr_t result = __rin_syscall6(
        SYS_EPOLL_PWAIT2, (uintptr_t)(unsigned int)epoll_fd,
        (uintptr_t)events, (uintptr_t)(unsigned int)max_events,
        (uintptr_t)timeout, (uintptr_t)signal_mask,
        (uintptr_t)sizeof(sigset_t));
    result = __rin_syscall_posixize(result);
    return result < 0 ? -1 : (int)result;
}
