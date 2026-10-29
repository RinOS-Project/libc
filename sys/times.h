/* SPDX-License-Identifier: MIT */
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
