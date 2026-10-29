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
    RinTimerSpecV1 previous = {0};
    struct itimerspec converted;
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
    error = _rin_time_timer_value_to_ns(
        &value->it_value, flags, &value_ns);
    if (error == 0)
        error = _rin_time_timespec_to_ns(&value->it_interval,
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
        (uintptr_t)(unsigned int)flags, (uintptr_t)&request,
        (uintptr_t)(old_value != NULL ? &previous : NULL)));
    if (result < 0) return -1;
    if (result != 0) {
        errno = EIO;
        return -1;
    }
    if (old_value != NULL) {
        if (!rin_timer_spec_v1_valid(&previous)) {
            errno = EIO;
            return -1;
        }
        error = _rin_time_ns_to_timespec(previous.value_ns,
                                         &converted.it_value);
        if (error == 0)
            error = _rin_time_ns_to_timespec(previous.interval_ns,
                                             &converted.it_interval);
        if (error != 0) {
            errno = error;
            return -1;
        }
        *old_value = converted;
    }
    return 0;
}

int timerfd_gettime(int fd, struct itimerspec* value)
{
    RinTimerSpecV1 result_wire = {0};
    struct itimerspec converted;
    intptr_t result;
    int error;
    if (fd < 0 || value == NULL) {
        errno = value == NULL ? EFAULT : EINVAL;
        return -1;
    }
    result = __rin_syscall_posixize(_syscall2(
        SYS_TIMERFD_GETTIME, (uintptr_t)(unsigned int)fd,
        (uintptr_t)&result_wire));
    if (result < 0) return -1;
    if (result != 0 || !rin_timer_spec_v1_valid(&result_wire)) {
        errno = EIO;
        return -1;
    }
    error = _rin_time_ns_to_timespec(result_wire.value_ns,
                                     &converted.it_value);
    if (error == 0)
        error = _rin_time_ns_to_timespec(result_wire.interval_ns,
                                         &converted.it_interval);
    if (error != 0) {
        errno = error;
        return -1;
    }
    *value = converted;
    return 0;
}
