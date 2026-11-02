/* SPDX-License-Identifier: MIT */
#if defined(__STDC_HOSTED__) && __STDC_HOSTED__ && \
    (!defined(RIN_FREESTANDING) || !(RIN_FREESTANDING)) && \
    defined(__linux__) && (defined(__GNUC__) || defined(__clang__)) && \
    !defined(RIN_TIME_CUSTOM_SYSCALL_HOOK) && \
    !defined(RIN_TIME_ABI64) && !defined(RIN_USERSPACE) && !defined(RIN_KERNEL) && \
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
    !defined(_RIN_TIMES_SYSCALL1) && \
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
#include "../limits.h"
#include "syscall.h"
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

#ifndef _RIN_TIMES_SYSCALL1
#if defined(_WIN32) && defined(__STDC_HOSTED__) && __STDC_HOSTED__ && \
    (!defined(RIN_FREESTANDING) || !(RIN_FREESTANDING))
#define _RIN_TIMES_SYSCALL1(number, argument1) \
    ((void)(number), (void)(argument1), -(intptr_t)ENOSYS)
#else
#define _RIN_TIMES_SYSCALL1(number, argument1) \
    _syscall1((uintptr_t)(number), (uintptr_t)(argument1))
#endif
#endif

static inline clock_t times(struct tms* buffer)
{
    intptr_t result;

    if (buffer == NULL) {
        errno = EFAULT;
        return (clock_t)-1;
    }
    result = __rin_syscall_posixize(_RIN_TIMES_SYSCALL1(
        SYS_TIMES_V1, (uintptr_t)buffer));
    if (result == -1)
        return (clock_t)-1;
    if (result < 0) {
        errno = EIO;
        return (clock_t)-1;
    }
    if ((uint64_t)result > (uint64_t)LONG_MAX) {
        errno = EOVERFLOW;
        return (clock_t)-1;
    }
    return (clock_t)result;
}

#ifdef __cplusplus
}
#endif

#endif /* _SYS_TIMES_H */
#endif /* hosted Linux delegates to the native times provider */
