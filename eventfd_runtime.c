/* SPDX-License-Identifier: MIT */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif
#include "sys/eventfd.h"

#include "limits.h"
#include "sys/syscall.h"
#include "unistd.h"

int eventfd2(unsigned int initial_value, int flags)
{
    intptr_t result = __rin_syscall_posixize(_syscall2(
        SYS_EVENTFD2, (uintptr_t)initial_value, (uintptr_t)(unsigned int)flags));

    if (result < 0) return -1;
    if (result > INT_MAX) {
        errno = EOVERFLOW;
        return -1;
    }
    return (int)result;
}

int eventfd(unsigned int initial_value, int flags)
{
    return eventfd2(initial_value, flags);
}

#if defined(_RIN_USE_GNU_EVENTFD)
int eventfd_read(int fd, eventfd_t* value)
{
    ssize_t transferred = read(fd, value, sizeof(*value));
    return transferred == (ssize_t)sizeof(*value) ? 0 : -1;
}

int eventfd_write(int fd, eventfd_t value)
{
    ssize_t transferred = write(fd, &value, sizeof(value));
    return transferred == (ssize_t)sizeof(value) ? 0 : -1;
}
#endif
