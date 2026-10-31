/* SPDX-License-Identifier: MIT */
#if defined(__STDC_HOSTED__) && __STDC_HOSTED__ && \
    (!defined(RIN_FREESTANDING) || !(RIN_FREESTANDING)) && \
    defined(__linux__) && (defined(__GNUC__) || defined(__clang__)) && \
    !defined(RIN_TIME_CUSTOM_SYSCALL_HOOK) && \
    !defined(_RIN_TIME_SYSCALL1) && !defined(_RIN_TIME_SYSCALL2) && \
    !defined(_RIN_TIME_SLEEP) && !defined(_RIN_TIME_GETTIMEOFDAY) && \
    !defined(_RIN_TIME_NS64) && !defined(_RIN_TIME_NS32) && \
    !defined(_RIN_TIME_REALTIME_NS64) && !defined(_RIN_TIME_REALTIME_NS32) && \
    !defined(_RIN_TIME_CLOCKSOURCE_RESOLUTION) && \
    !defined(_RIN_TIME_CPU_GET) && !defined(_RIN_TIME_SETITIMER) && \
    !defined(_RIN_TIME_GETITIMER) && \
    !defined(_RIN_TIME_TIMER_CREATE) && !defined(_RIN_TIME_TIMER_SETTIME) && \
    !defined(_RIN_TIME_TIMER_GETTIME) && !defined(_RIN_TIME_TIMER_DELETE) && \
    !defined(_RIN_TIME_TIMER_GETOVERRUN) && \
    !defined(_RIN_TIME_TIMER_POLL) && !defined(_RIN_TIME_CLOCK_SETTIME) && \
    !defined(_RIN_TIME_CLOCK_GETTIME64) && !defined(_RIN_TIME_FUTEX_WAIT) && \
    !defined(RIN_RESOURCE_LIMIT_PRODUCT_SYSCALL) && \
    !defined(RIN_PROCESS_PRIORITY_PRODUCT_SYSCALL) && \
    !defined(_RIN_RESOURCE_LIMIT_SYSCALL2) && \
    !defined(_RIN_PRIORITY_SYSCALL2) && !defined(_RIN_PRIORITY_SYSCALL3) && \
    !defined(_RIN_GETRUSAGE_SYSCALL2)
#include_next <sys/times.h>
#else
#ifndef _SYS_TIMES_H
#define _SYS_TIMES_H

#include "../errno.h"
#include "../internal/posix_clock.h"
#include "../limits.h"
#include "../sys/resource.h"
#include "../time.h"
#include "../unistd.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _RIN_TMS_DEFINED
#define _RIN_TMS_DEFINED
struct tms {
    clock_t tms_utime;
    clock_t tms_stime;
    clock_t tms_cutime;
    clock_t tms_cstime;
};
#endif

/* 秒と小数部を公開tickへ変換し、clock_tの上限を越える場合は失敗する。 */
static inline int __rin_times_to_ticks(uint64_t seconds,
                                       uint64_t fraction,
                                       uint64_t fraction_base,
                                       clock_t* ticks_out)
{
    const uint64_t maximum = (uint64_t)LONG_MAX;
    uint64_t whole_ticks;
    uint64_t fractional_ticks;
    if (ticks_out == NULL || fraction_base == 0u ||
        fraction >= fraction_base) {
        errno = EIO;
        return -1;
    }
    if (seconds > maximum / (uint64_t)RIN_POSIX_CLK_TCK) {
        errno = EOVERFLOW;
        return -1;
    }
    whole_ticks = seconds * (uint64_t)RIN_POSIX_CLK_TCK;
    fractional_ticks = (fraction * (uint64_t)RIN_POSIX_CLK_TCK) /
                       fraction_base;
    if (whole_ticks > maximum - fractional_ticks) {
        errno = EOVERFLOW;
        return -1;
    }
    *ticks_out = (clock_t)(whole_ticks + fractional_ticks);
    return 0;
}

static inline int __rin_times_timeval_to_ticks(const struct timeval* value,
                                               clock_t* ticks_out)
{
    if (value == NULL || value->tv_sec < 0 || value->tv_usec < 0 ||
        value->tv_usec >= 1000000L) {
        errno = EIO;
        return -1;
    }
    return __rin_times_to_ticks((uint64_t)value->tv_sec,
                                (uint64_t)value->tv_usec,
                                UINT64_C(1000000), ticks_out);
}

static inline clock_t times(struct tms* buffer)
{
    struct rusage self_usage;
    struct rusage child_usage;
    struct timespec elapsed;
    struct tms result;
    clock_t elapsed_ticks;

    if (buffer == NULL) {
        errno = EFAULT;
        return (clock_t)-1;
    }
    if (getrusage(RUSAGE_SELF, &self_usage) != 0 ||
        getrusage(RUSAGE_CHILDREN, &child_usage) != 0 ||
        clock_gettime(CLOCK_MONOTONIC, &elapsed) != 0)
        return (clock_t)-1;
    if (__rin_times_timeval_to_ticks(&self_usage.ru_utime,
                                     &result.tms_utime) != 0 ||
        __rin_times_timeval_to_ticks(&self_usage.ru_stime,
                                     &result.tms_stime) != 0 ||
        __rin_times_timeval_to_ticks(&child_usage.ru_utime,
                                     &result.tms_cutime) != 0 ||
        __rin_times_timeval_to_ticks(&child_usage.ru_stime,
                                     &result.tms_cstime) != 0)
        return (clock_t)-1;
    if (elapsed.tv_sec < 0 || elapsed.tv_nsec < 0 ||
        elapsed.tv_nsec >= 1000000000L) {
        errno = EIO;
        return (clock_t)-1;
    }
    if (__rin_times_to_ticks((uint64_t)elapsed.tv_sec,
                             (uint64_t)elapsed.tv_nsec,
                             UINT64_C(1000000000), &elapsed_ticks) != 0)
        return (clock_t)-1;

    *buffer = result;
    return elapsed_ticks;
}

#ifdef __cplusplus
}
#endif

#endif /* _SYS_TIMES_H */
#endif /* hosted Linux delegates to the native times provider */
