/* SPDX-License-Identifier: MIT */

#include "sys/timerfd.h"

#include "limits.h"
#include "sys/syscall.h"

#include <rin/time/timer_abi.h>

int timerfd_create(clockid_t clock_id, int flags)
{
    intptr_t result;
    if (flags & ~(TFD_NONBLOCK | TFD_CLOEXEC)) {
        errno = EINVAL;
        return -1;
    }
    result = __rin_syscall_posixize(_syscall2(
        SYS_TIMERFD_CREATE, (uintptr_t)(intptr_t)clock_id,
        (uintptr_t)(unsigned int)flags));
    if (result < 0) return -1;
    if (result > INT_MAX) {
        errno = EOVERFLOW;
        return -1;
    }
    return (int)result;
}

int timerfd_settime(int fd, int flags, const struct itimerspec* value,
                    struct itimerspec* old_value)
{
    RinTimerSpecV1 request = {0};
    struct itimerspec value_snapshot;
    intptr_t result;
    int error;
    uint64_t value_ns;
    uint64_t interval_ns;

    if (fd < 0 || value == NULL) {
        errno = value == NULL ? EFAULT : EINVAL;
        return -1;
    }
    if ((flags & ~(TFD_TIMER_ABSTIME | TFD_TIMER_CANCEL_ON_SET)) != 0 ||
        ((flags & TFD_TIMER_CANCEL_ON_SET) != 0 &&
         (flags & TFD_TIMER_ABSTIME) == 0)) {
        errno = EINVAL;
        return -1;
    }
    if (__rin_user_snapshot(value, sizeof(value_snapshot), 0,
                            &value_snapshot) != 0)
        return -1;
    error = _rin_time_timer_value_to_ns(
        &value_snapshot.it_value, flags, &value_ns);
    if (error == 0)
        error = _rin_time_timespec_to_ns(&value_snapshot.it_interval,
                                         &interval_ns);
    if (error != 0) {
        errno = error;
        return -1;
    }

    request.struct_size = (uint32_t)sizeof(request);
    request.version = (uint16_t)RIN_TIMER_ABI_VERSION;
    request.flags = 0u;
    request.value_ns = value_ns;
    request.interval_ns = interval_ns;
    request.reserved0 = 0u;
    request.reserved1 = 0u;
    result = __rin_syscall_posixize(_syscall4(
        SYS_TIMERFD_SETTIME, (uintptr_t)(unsigned int)fd,
        (uintptr_t)((uint32_t)flags | __RIN_TIMER_USERPTR_TIME_MODE),
        (uintptr_t)&request, (uintptr_t)old_value));
    if (result < 0) return -1;
    if (result != 0) {
        errno = EIO;
        return -1;
    }
    return 0;
}

int timerfd_gettime(int fd, struct itimerspec* value)
{
    intptr_t result;
    if (fd < 0 || value == NULL) {
        errno = value == NULL ? EFAULT : EINVAL;
        return -1;
    }
    result = __rin_syscall_posixize(_syscall2(
        SYS_TIMERFD_GETTIME,
        (uintptr_t)((uint32_t)fd | __RIN_TIMER_USERPTR_TIME_MODE),
        (uintptr_t)value));
    if (result < 0) return -1;
    if (result != 0) {
        errno = EIO;
        return -1;
    }
    return 0;
}
