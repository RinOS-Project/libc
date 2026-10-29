/* SPDX-License-Identifier: MIT */

#include "sys/eventfd.h"

#include "limits.h"
#include "sys/syscall.h"

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
