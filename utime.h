/* SPDX-License-Identifier: MIT */
/*
 * RinOS libc - utime.h
 * Minimal timestamp update compatibility.
 */

#ifndef _UTIME_H
#define _UTIME_H

#include "errno.h"
#include "fcntl.h"
#include "time.h"
#include "sys/time.h"
#include <rin/time/futimens_abi.h>
#include <rin/fs/path_at_abi.h>

/* A hosted dependency can claim the generic _TIME_H include guard without
 * exporting timeval.  Keep the POSIX timestamp API self-contained while
 * respecting the system guard when it did provide the type. */
#if !defined(_TIMEVAL_DEFINED) && !defined(__timeval_defined)
#define _TIMEVAL_DEFINED
struct timeval {
    time_t tv_sec;
    long tv_usec;
};
#endif

#ifndef _RIN_UTIME_SYSCALL2
#define _RIN_UTIME_SYSCALL2(number, argument1, argument2) \
    _syscall2((uintptr_t)(number), (uintptr_t)(argument1), \
              (uintptr_t)(argument2))
#endif
#ifndef _RIN_UTIME_SYSCALL3
#define _RIN_UTIME_SYSCALL3(number, argument1, argument2, argument3) \
    _syscall3((uintptr_t)(number), (uintptr_t)(argument1), \
              (uintptr_t)(argument2), (uintptr_t)(argument3))
#endif
#ifndef _RIN_UTIME_PATH_AT_CALL
#define _RIN_UTIME_PATH_AT_CALL(call) \
    _syscall1((uintptr_t)SYS_PATH_AT, (uintptr_t)(call))
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct utimbuf {
    time_t actime;
    time_t modtime;
};

#ifndef UTIME_NOW
#define UTIME_NOW RIN_FUTIMENS_NSEC_NOW
#endif
#ifndef UTIME_OMIT
#define UTIME_OMIT RIN_FUTIMENS_NSEC_OMIT
#endif

#ifndef MIDL_PASS
static inline int __rin_utime_result(intptr_t result) {
    uintptr_t error;
    if (result < 0) {
        error = (uintptr_t)0 - (uintptr_t)result;
        errno = error != 0u && error <= 4095u ? (int)error : EIO;
        return -1;
    }
    if (result != 0) {
        errno = EIO;
        return -1;
    }
    return 0;
}

static inline int __rin_utime_prepare_call(
    RinFutimensCallV1* call, const struct timespec times[2]) {
    if (call == (RinFutimensCallV1*)0) {
        errno = EFAULT;
        return -1;
    }
    for (size_t index = 0u; index < sizeof(*call); ++index)
        ((unsigned char*)call)[index] = 0u;
    call->struct_size = (uint32_t)sizeof(*call);
    call->version = RIN_FUTIMENS_CALL_VERSION;
    if (times == (const struct timespec*)0) {
        call->flags = RIN_FUTIMENS_FLAG_TIMES_NULL;
        return 0;
    }
    for (size_t index = 0u; index < 2u; ++index) {
        long nanoseconds = times[index].tv_nsec;
        if (!((nanoseconds >= 0 && nanoseconds <= 999999999L) ||
              nanoseconds == (long)UTIME_NOW ||
              nanoseconds == (long)UTIME_OMIT)) {
            errno = EINVAL;
            return -1;
        }
    }
    call->access_seconds = (int64_t)times[0].tv_sec;
    call->access_nanoseconds = (int32_t)times[0].tv_nsec;
    call->write_seconds = (int64_t)times[1].tv_sec;
    call->write_nanoseconds = (int32_t)times[1].tv_nsec;
    return 0;
}

static inline void __rin_utime_path_at_init(RinPathAtCallV1* call) {
    size_t index;
    for (index = 0u; index < sizeof(*call); ++index)
        ((unsigned char*)call)[index] = 0u;
    call->struct_size = (uint32_t)sizeof(*call);
    call->version = RIN_PATH_AT_CALL_VERSION;
    call->operation = RIN_PATH_AT_UTIMENS;
    call->dirfd = AT_FDCWD;
    call->secondary_dirfd = AT_FDCWD;
}

static inline int __rin_utimens_path(
    int dirfd, const char* pathname, const struct timespec times[2],
    int flags) {
    RinFutimensCallV1 call;
    RinPathAtCallV1 path_at;
    if (pathname == (const char*)0) {
        errno = EFAULT;
        return -1;
    }
    if (pathname[0] == '\0') {
        errno = ENOENT;
        return -1;
    }
    if ((flags & ~(int)AT_SYMLINK_NOFOLLOW) != 0) {
        errno = EINVAL;
        return -1;
    }
    if (__rin_utime_prepare_call(&call, times) != 0) return -1;
    if (pathname[0] != '/' && dirfd != AT_FDCWD) {
        __rin_utime_path_at_init(&path_at);
        path_at.flags = (uint32_t)flags;
        path_at.dirfd = dirfd;
        path_at.path1 = (uint64_t)(uintptr_t)pathname;
        path_at.buffer = (uint64_t)(uintptr_t)&call;
        path_at.buffer_size = sizeof(call);
        return __rin_utime_result(_RIN_UTIME_PATH_AT_CALL(&path_at));
    }
    return __rin_utime_result(_RIN_UTIME_SYSCALL3(
        SYS_UTIMENSAT, pathname, &call, flags));
}

static inline int utime(const char* path, const struct utimbuf* times) {
    struct timespec values[2];
    if (times == (const struct utimbuf*)0)
        return __rin_utimens_path(AT_FDCWD, path,
                                  (const struct timespec*)0, 0);
    values[0].tv_sec = times->actime;
    values[0].tv_nsec = 0;
    values[1].tv_sec = times->modtime;
    values[1].tv_nsec = 0;
    return __rin_utimens_path(AT_FDCWD, path, values, 0);
}

static inline int __rin_utimes_path(const char* path,
                                    const struct timeval times[2],
                                    int flags) {
    struct timespec values[2];
    if (times == (const struct timeval*)0)
        return __rin_utimens_path(AT_FDCWD, path,
                                  (const struct timespec*)0, flags);
    for (size_t index = 0u; index < 2u; ++index) {
        if (times[index].tv_usec < 0 || times[index].tv_usec > 999999) {
            errno = EINVAL;
            return -1;
        }
        values[index].tv_sec = times[index].tv_sec;
        values[index].tv_nsec = (long)times[index].tv_usec * 1000L;
    }
    return __rin_utimens_path(AT_FDCWD, path, values, flags);
}

static inline int utimes(const char* path, const struct timeval times[2]) {
    return __rin_utimes_path(path, times, 0);
}

static inline int lutimes(const char* path, const struct timeval times[2]) {
    return __rin_utimes_path(path, times, AT_SYMLINK_NOFOLLOW);
}

static inline int futimens(int fd, const struct timespec times[2]) {
    RinFutimensCallV1 call;
    intptr_t result;

    if (fd < 0) {
        errno = EBADF;
        return -1;
    }
    if (__rin_utime_prepare_call(&call, times) != 0) return -1;
    result = _RIN_UTIME_SYSCALL2(SYS_FUTIMENS, fd, &call);
    return __rin_utime_result(result);
}

static inline int utimensat(int dirfd, const char* pathname, const struct timespec times[2], int flags) {
    return __rin_utimens_path(dirfd, pathname, times, flags);
}
#endif /* !MIDL_PASS */

#ifdef __cplusplus
}
#endif

#endif /* _UTIME_H */
