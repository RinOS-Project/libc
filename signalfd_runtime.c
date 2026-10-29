/* SPDX-License-Identifier: MIT */

#include "sys/signalfd.h"

#include "limits.h"
#include "sys/syscall.h"

int signalfd4(int fd, const sigset_t* mask, int sizemask, int flags)
{
    intptr_t result;
    if (mask == NULL) {
        errno = EFAULT;
        return -1;
    }
    if ((flags & ~(SFD_NONBLOCK | SFD_CLOEXEC)) != 0) {
        errno = EINVAL;
        return -1;
    }
    result = __rin_syscall_posixize(_syscall4(
        SYS_SIGNALFD4, (uintptr_t)(unsigned int)fd, (uintptr_t)mask,
        (uintptr_t)(unsigned int)sizemask,
        (uintptr_t)(unsigned int)flags));
    if (result < 0) return -1;
    if (result > INT_MAX) {
        errno = EOVERFLOW;
        return -1;
    }
    return (int)result;
}

int signalfd(int fd, const sigset_t* mask, int flags)
{
    return signalfd4(fd, mask, (int)sizeof(sigset_t), flags);
}
