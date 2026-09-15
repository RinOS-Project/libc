/*
 * RinOS libc - sys/time.h
 * BSD互換時間関数
 */

#ifndef _SYS_TIME_H
#define _SYS_TIME_H

/* time.hに定義を集約 */
#include "../time.h"

#define ITIMER_REAL    0
#define ITIMER_VIRTUAL 1
#define ITIMER_PROF    2

#ifndef _ITIMERVAL_DEFINED
#define _ITIMERVAL_DEFINED
struct itimerval {
    struct timeval it_interval;
    struct timeval it_value;
};
#endif

/* A custom hook is used by linked contract tests and by the freestanding
 * target.  Hosted builds deliberately retain the explicit ENOSYS boundary. */
#ifndef _RIN_TIME_SETITIMER
#define _RIN_TIME_SETITIMER(which, new_value, old_value) \
    _syscall3((uintptr_t)SYS_SETITIMER, (uintptr_t)(which), \
              (uintptr_t)(new_value), (uintptr_t)(old_value))
#define RIN_TIME_SETITIMER_DEFAULT_HOOK 1
#endif

#ifndef _RIN_TIME_GETITIMER
#define _RIN_TIME_GETITIMER(which, value) \
    _syscall2((uintptr_t)SYS_GETITIMER, (uintptr_t)(which), \
              (uintptr_t)(value))
#define RIN_TIME_GETITIMER_DEFAULT_HOOK 1
#endif

#ifndef MIDL_PASS
static inline int _rin_itimer_timeval_to_us(const struct timeval* value,
                                            uint64_t* output) {
    uint64_t seconds;
    uint64_t usec;
    if (!value || !output) return EINVAL;
    if (value->tv_sec < 0 || value->tv_usec < 0 || value->tv_usec >= 1000000L)
        return EINVAL;
    seconds = (uint64_t)value->tv_sec;
    usec = (uint64_t)value->tv_usec;
    if (seconds > (UINT64_MAX - usec) / UINT64_C(1000000))
        return EOVERFLOW;
    *output = seconds * UINT64_C(1000000) + usec;
    return 0;
}

static inline int _rin_itimer_us_to_timeval(uint64_t value_us,
                                            struct timeval* output) {
    uint64_t seconds;
    if (!output) return EFAULT;
    seconds = value_us / UINT64_C(1000000);
    if (seconds > (uint64_t)LONG_MAX) return EOVERFLOW;
    output->tv_sec = (time_t)seconds;
    output->tv_usec = (long)(value_us % UINT64_C(1000000));
    return 0;
}

static inline int setitimer(int which, const struct itimerval* new_value,
                            struct itimerval* old_value) {
    RinItimerV1 request;
    RinItimerV1 previous;
    uint64_t value_us;
    uint64_t interval_us;
    int error;
    intptr_t result;

    if (which < ITIMER_REAL || which > ITIMER_PROF) {
        errno = EINVAL;
        return -1;
    }
    if (!new_value) {
        errno = EFAULT;
        return -1;
    }
    error = _rin_itimer_timeval_to_us(&new_value->it_value, &value_us);
    if (error != 0) {
        errno = error;
        return -1;
    }
    error = _rin_itimer_timeval_to_us(&new_value->it_interval, &interval_us);
    if (error != 0) {
        errno = error;
        return -1;
    }
    request.struct_size = (uint32_t)sizeof(request);
    request.version = (uint16_t)RIN_ITIMER_ABI_VERSION;
    request.reserved0 = 0u;
    request.value_us = value_us;
    request.interval_us = interval_us;
    request.reserved = 0u;
    if (old_value) {
        previous.struct_size = 0u;
        previous.version = 0u;
        previous.reserved0 = 0u;
        previous.value_us = 0u;
        previous.interval_us = 0u;
        previous.reserved = 0u;
    }
#if defined(RIN_FREESTANDING) || !defined(RIN_TIME_SETITIMER_DEFAULT_HOOK)
    result = _RIN_TIME_SETITIMER(which, &request,
                                  old_value ? &previous : (void*)0);
    if (_rin_time_status(result) != 0) return -1;
#else
    (void)result;
    errno = ENOSYS;
    return -1;
#endif
    if (old_value) {
        error = _rin_itimer_us_to_timeval(previous.value_us,
                                           &old_value->it_value);
        if (error != 0) {
            errno = error;
            return -1;
        }
        error = _rin_itimer_us_to_timeval(previous.interval_us,
                                           &old_value->it_interval);
        if (error != 0) {
            errno = error;
            return -1;
        }
    }
    return 0;
}

static inline int getitimer(int which, struct itimerval* value) {
    RinItimerV1 current;
    struct timeval converted_value;
    struct timeval converted_interval;
    int error;
    intptr_t result;

    if (which < ITIMER_REAL || which > ITIMER_PROF) {
        errno = EINVAL;
        return -1;
    }
    if (!value) {
        errno = EFAULT;
        return -1;
    }
    current.struct_size = 0u;
    current.version = 0u;
    current.reserved0 = 0u;
    current.value_us = 0u;
    current.interval_us = 0u;
    current.reserved = 0u;
#if defined(RIN_FREESTANDING) || !defined(RIN_TIME_GETITIMER_DEFAULT_HOOK)
    result = _RIN_TIME_GETITIMER(which, &current);
    error = _rin_time_status(result);
    if (error != 0) return -1;
#else
    (void)result;
    errno = ENOSYS;
    return -1;
#endif
    if (current.struct_size != (uint32_t)sizeof(current) ||
        current.version != (uint16_t)RIN_ITIMER_ABI_VERSION ||
        current.reserved0 != 0u || current.reserved != 0u) {
        errno = EIO;
        return -1;
    }
    error = _rin_itimer_us_to_timeval(current.value_us, &converted_value);
    if (error != 0) {
        errno = error;
        return -1;
    }
    error = _rin_itimer_us_to_timeval(current.interval_us,
                                      &converted_interval);
    if (error != 0) {
        errno = error;
        return -1;
    }
    value->it_value = converted_value;
    value->it_interval = converted_interval;
    return 0;
}

#endif /* !MIDL_PASS */

/* timezone構造体 (BSD互換) */
#ifndef _TIMEZONE_DEFINED
#define _TIMEZONE_DEFINED
struct timezone {
    int tz_minuteswest;
    int tz_dsttime;
};
#endif

#include "../utime.h"

#endif /* _SYS_TIME_H */
