/* SPDX-License-Identifier: MIT */
/*
 * RinOS libc - poll.h
 * I/O多重化
 */

#if defined(__STDC_HOSTED__) && __STDC_HOSTED__ && \
    (!defined(RIN_FREESTANDING) || !(RIN_FREESTANDING)) && \
    defined(__linux__) && (defined(__GNUC__) || defined(__clang__)) && \
    !defined(_RIN_POLL_SYSCALL5)
#include_next <poll.h>
#else

#ifndef _POLL_H
#define _POLL_H

#include "features.h"
#include "stddef.h"
#include "stdint.h"
#include "errno.h"
#include "limits.h"
#include "time.h"
#include "sys/types.h"
#include "sys/syscall.h"
#include "signal.h"
#include "stdlib.h"
#include "unistd.h"
#include "sys/uio.h"
#include "internal/pthread_cancel_point.h"
#include <rin/poll/ppoll_timeout_abi.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════
 * poll構造体
 * ═══════════════════════════════════════════════════════════════*/

struct pollfd {
    int   fd;       /* ファイルディスクリプタ */
    short events;   /* 監視するイベント */
    short revents;  /* 発生したイベント */
};

typedef unsigned int nfds_t;

#ifndef _SIGSET_T_DEFINED
#define _SIGSET_T_DEFINED
typedef uint64_t sigset_t;
#endif

/* ═══════════════════════════════════════════════════════════════
 * pollイベントフラグ
 * ═══════════════════════════════════════════════════════════════*/

/* 入力イベント (events, revents) */
#define POLLIN      0x0001  /* 読み取りデータあり */
#define POLLPRI     0x0002  /* 優先データあり */
#define POLLOUT     0x0004  /* 書き込み可能 */

/* 出力イベント (reventsのみ) */
#define POLLERR     0x0008  /* エラー */
#define POLLHUP     0x0010  /* 切断 */
#define POLLNVAL    0x0020  /* 無効なfd */

/* 追加イベント */
#define POLLRDNORM  0x0040  /* 通常データ読み取り可能 */
#define POLLRDBAND  0x0080  /* 優先バンドデータ読み取り可能 */
#define POLLWRNORM  0x0100  /* 通常データ書き込み可能 */
#define POLLWRBAND  0x0200  /* 優先バンドデータ書き込み可能 */
#define POLLMSG     0x0400  /* メッセージあり */
#define POLLREMOVE  0x1000  /* 削除要求 */
#define POLLRDHUP   0x2000  /* ピア切断 */

/* Keep the libc bound aligned with the kernel PPOLL ABI's 1024-entry limit so
 * oversized requests fail before entering the syscall. */
#ifndef RIN_POLL_MAX_FDS
#define RIN_POLL_MAX_FDS 1024u
#endif

/* ═══════════════════════════════════════════════════════════════
 * poll関数
 * ═══════════════════════════════════════════════════════════════*/

#ifndef _RIN_POLL_SYSCALL5
#define _RIN_POLL_SYSCALL5(number, argument1, argument2, argument3, \
                          argument4, argument5) \
    _syscall5((uintptr_t)(number), (uintptr_t)(argument1), \
              (uintptr_t)(argument2), (uintptr_t)(argument3), \
              (uintptr_t)(argument4), (uintptr_t)(argument5))
#endif

#ifndef _RIN_POLL_SYSCALL1
#define _RIN_POLL_SYSCALL1(number, argument1) \
    _syscall1((uintptr_t)(number), (uintptr_t)(argument1))
#endif

static inline intptr_t __rin_poll_result(intptr_t result) {
    result = __rin_syscall_posixize(result);
    if (result < 0) {
        if (result != -1) errno = EIO;
        return -1;
    }
    return result;
}

static inline int __rin_ppoll_syscall_once(
    struct pollfd* fds, nfds_t nfds, int64_t timeout_ns,
    const sigset_t* sigmask) {
    RinPpollTimeout64UserV1 request = {0};
    intptr_t result;
    request.struct_size = (uint32_t)sizeof(request);
    request.version = RIN_PPOLL_TIMEOUT64_USER_ABI_VERSION;
    request.nfds = nfds;
    request.sigset_size = RIN_PPOLL_TIMEOUT64_USER_SIGSET_SIZE;
    request.timeout_ns = timeout_ns;
    request.fds_address = (uint64_t)(uintptr_t)fds;
    request.sigmask_address = (uint64_t)(uintptr_t)sigmask;
    result = __rin_poll_result(_RIN_POLL_SYSCALL1(
        SYS_PPOLL_TIMEOUT64_V1, &request));
    if (result < 0) return -1;
    if (result > (intptr_t)INT_MAX) {
        errno = EOVERFLOW;
        return -1;
    }
    return (int)result;
}

static inline int __rin_poll_syscall_without_mask_change(
    struct pollfd* fds, nfds_t nfds, int64_t timeout_ns) {
    return __rin_ppoll_syscall_once(fds, nfds, timeout_ns, NULL);
}

static inline int __rin_select_syscall_once(
    int nfds, fd_set* readfds, fd_set* writefds, fd_set* exceptfds,
    int64_t timeout_ns, const sigset_t* sigmask) {
    RinPpollTimeout64UserV1 request = {0};
    intptr_t result;
    const size_t fdset_size = sizeof(fd_set);
    if (nfds < 0 || nfds > (int)RIN_POLL_MAX_FDS ||
        fdset_size == 0u ||
        fdset_size > RIN_PPOLL_TIMEOUT64_USER_FDSET_MAX_BYTES ||
        (size_t)nfds > fdset_size * 8u) {
        errno = EINVAL;
        return -1;
    }
    request.struct_size = (uint32_t)sizeof(request);
    request.version = RIN_PPOLL_TIMEOUT64_USER_ABI_VERSION;
    request.flags = RIN_PPOLL_TIMEOUT64_USER_SELECT_FLAGS(fdset_size);
    request.nfds = (uint32_t)nfds;
    request.sigset_size = RIN_PPOLL_TIMEOUT64_USER_SIGSET_SIZE;
    request.timeout_ns = timeout_ns;
    request.fds_address = (uint64_t)(uintptr_t)readfds;
    request.sigmask_address = (uint64_t)(uintptr_t)sigmask;
    request.reserved[0] = (uint64_t)(uintptr_t)writefds;
    request.reserved[1] = (uint64_t)(uintptr_t)exceptfds;
    result = __rin_poll_result(_RIN_POLL_SYSCALL1(
        SYS_PPOLL_TIMEOUT64_V1, &request));
    if (result < 0) return -1;
    if (result > (intptr_t)INT_MAX) {
        errno = EOVERFLOW;
        return -1;
    }
    return (int)result;
}

static inline int __rin_poll_timespec_to_ns(
    const struct timespec* timeout, int64_t* timeout_ns) {
    if (!timeout_ns) {
        errno = EINVAL;
        return -1;
    }
    if (!timeout) {
        *timeout_ns = -1;
        return 0;
    }
    if (timeout->tv_sec < 0 || timeout->tv_nsec < 0 ||
        timeout->tv_nsec >= 1000000000L) {
        errno = EINVAL;
        return -1;
    }
    if ((uint64_t)timeout->tv_sec >
        (uint64_t)INT64_MAX / UINT64_C(1000000000)) {
        errno = EOVERFLOW;
        return -1;
    }
    {
        const int64_t seconds_ns =
            (int64_t)timeout->tv_sec * INT64_C(1000000000);
        if ((int64_t)timeout->tv_nsec > INT64_MAX - seconds_ns) {
            errno = EOVERFLOW;
            return -1;
        }
        *timeout_ns = seconds_ns + (int64_t)timeout->tv_nsec;
    }
    if (*timeout_ns < 0) {
        errno = EOVERFLOW;
        return -1;
    }
    return 0;
}

static inline int __rin_poll_timeval_to_ns(
    const struct timeval* timeout, int64_t* timeout_ns) {
    if (!timeout_ns) {
        errno = EINVAL;
        return -1;
    }
    if (!timeout) {
        *timeout_ns = -1;
        return 0;
    }
    if (timeout->tv_sec < 0 || timeout->tv_usec < 0 ||
        timeout->tv_usec >= 1000000L) {
        errno = EINVAL;
        return -1;
    }
    if ((uint64_t)timeout->tv_sec >
        (uint64_t)INT64_MAX / UINT64_C(1000000000)) {
        errno = EOVERFLOW;
        return -1;
    }
    {
        const int64_t seconds_ns =
            (int64_t)timeout->tv_sec * INT64_C(1000000000);
        const int64_t microseconds_ns =
            (int64_t)timeout->tv_usec * INT64_C(1000);
        if (microseconds_ns > INT64_MAX - seconds_ns) {
            errno = EOVERFLOW;
            return -1;
        }
        *timeout_ns = seconds_ns + microseconds_ns;
    }
    if (*timeout_ns < 0) {
        errno = EOVERFLOW;
        return -1;
    }
    return 0;
}

static inline int __rin_poll_snapshot_timespec(
    const struct timespec* timeout, struct timespec* snapshot,
    int64_t* timeout_ns) {
    int result;
    if (!timeout) return __rin_poll_timespec_to_ns(NULL, timeout_ns);
    result = _rin_time_snapshot_user_timespec(
        RIN_CLOCK_ID_MONOTONIC, timeout, snapshot);
    if (result != 0) {
        errno = result;
        return -1;
    }
    return __rin_poll_timespec_to_ns(snapshot, timeout_ns);
}

static inline int __rin_poll_snapshot_timeval(
    const struct timeval* timeout, struct timeval* snapshot,
    int64_t* timeout_ns) {
    struct timespec wire_snapshot;
    int result;
    if (!timeout) return __rin_poll_timeval_to_ns(NULL, timeout_ns);
    result = _rin_time_snapshot_user_timespec(
        RIN_CLOCK_ID_MONOTONIC,
        (const struct timespec*)(const void*)timeout,
        &wire_snapshot);
    if (result != 0) {
        errno = result;
        return -1;
    }
    if (wire_snapshot.tv_nsec >= 1000000L) {
        errno = EINVAL;
        return -1;
    }
    snapshot->tv_sec = (time_t)wire_snapshot.tv_sec;
    snapshot->tv_usec = (long)wire_snapshot.tv_nsec;
    return __rin_poll_timeval_to_ns(snapshot, timeout_ns);
}

/* Linux updates select()'s timeval with the unspent portion of the original
 * timeout. Measure only the poll wait, using CLOCK_MONOTONIC so wall-clock
 * adjustments cannot extend or shorten it. */
static inline int __rin_select_update_timeval(
        struct timeval* timeout, const struct timeval* original_timeout,
        const struct timespec* start) {
    const uint64_t nanoseconds_per_second = UINT64_C(1000000000);
    struct timespec now;
    struct timeval remaining;
    struct iovec local_iov;
    struct iovec remote_iov;
    uint64_t requested_ns;
    uint64_t elapsed_seconds;
    uint64_t elapsed_ns;
    uint64_t remaining_ns;
    int saved_errno;
    ssize_t copied;

    if (!timeout || !original_timeout || !start) return 0;
    saved_errno = errno;
    if (__rin_time_clock_gettime(RIN_CLOCK_ID_MONOTONIC, &now) != 0)
        return -1;
    if (start->tv_sec < 0 || start->tv_nsec < 0 ||
        start->tv_nsec >= 1000000000L || now.tv_sec < 0 ||
        now.tv_nsec < 0 || now.tv_nsec >= 1000000000L) {
        errno = EIO;
        return -1;
    }

    requested_ns = (uint64_t)original_timeout->tv_sec *
                       nanoseconds_per_second +
                   (uint64_t)original_timeout->tv_usec * UINT64_C(1000);
    if (now.tv_sec < start->tv_sec ||
        (now.tv_sec == start->tv_sec && now.tv_nsec < start->tv_nsec)) {
        elapsed_ns = 0u;
    } else {
        elapsed_seconds = (uint64_t)(now.tv_sec - start->tv_sec);
        if (now.tv_nsec < start->tv_nsec) {
            if (elapsed_seconds != 0u) --elapsed_seconds;
            elapsed_ns = nanoseconds_per_second +
                         (uint64_t)now.tv_nsec -
                         (uint64_t)start->tv_nsec;
        } else {
            elapsed_ns = (uint64_t)now.tv_nsec -
                         (uint64_t)start->tv_nsec;
        }
        if (elapsed_seconds >
            (UINT64_MAX - elapsed_ns) / nanoseconds_per_second) {
            elapsed_ns = UINT64_MAX;
        } else {
            elapsed_ns += elapsed_seconds * nanoseconds_per_second;
        }
    }

    remaining_ns = elapsed_ns >= requested_ns
        ? 0u : requested_ns - elapsed_ns;
    remaining.tv_sec = (time_t)(remaining_ns / nanoseconds_per_second);
    remaining.tv_usec = (long)((remaining_ns % nanoseconds_per_second) /
                               UINT64_C(1000));
    local_iov.iov_base = &remaining;
    local_iov.iov_len = sizeof(remaining);
    remote_iov.iov_base = timeout;
    remote_iov.iov_len = sizeof(remaining);
    copied = process_vm_writev(getpid(), &local_iov, 1u, &remote_iov, 1u, 0u);
    if (copied < 0) return -1;
    if ((size_t)copied != sizeof(remaining)) {
        errno = EFAULT;
        return -1;
    }
    errno = saved_errno;
    return 0;
}

/* poll - ファイルディスクリプタを監視
 * 戻り値: イベントが発生したfdの数、0=タイムアウト、-1=エラー
 */
static inline int poll(struct pollfd* fds, nfds_t nfds, int timeout) {
    int64_t timeout_ns;
    __rin_pthread_cancel_point_if_linked();
    if (nfds > RIN_POLL_MAX_FDS || (fds == NULL && nfds != 0u)) {
        errno = fds == NULL && nfds != 0u ? EFAULT : EINVAL;
        return -1;
    }
    timeout_ns = timeout < 0 ? -1 : (int64_t)timeout * INT64_C(1000000);
    {
        int result = __rin_poll_syscall_without_mask_change(
            fds, nfds, timeout_ns);
        __rin_pthread_cancel_point_if_linked();
        return result;
    }
}

/* ppoll - シグナルマスク付きpoll */
static inline int __rin_ppoll_impl(struct pollfd* fds, nfds_t nfds,
                                   const struct timespec* timeout_ts,
                                   const sigset_t* sigmask) {
    int64_t timeout_ns;
    struct timespec timeout_snapshot;
    int result;
    __rin_pthread_cancel_point_if_linked();

    if (nfds > RIN_POLL_MAX_FDS || (fds == NULL && nfds != 0u)) {
        errno = fds == NULL && nfds != 0u ? EFAULT : EINVAL;
        return -1;
    }
    if (__rin_poll_snapshot_timespec(
            timeout_ts, &timeout_snapshot, &timeout_ns) != 0)
        return -1;
    result = __rin_ppoll_syscall_once(fds, nfds, timeout_ns, sigmask);
    __rin_pthread_cancel_point_if_linked();
    return result;
}

#if defined(__RIN_FEATURES_POSIX_2024) || \
    defined(__RIN_FEATURES_GNU) || defined(__RIN_FEATURES_DEFAULT)
static inline int ppoll(struct pollfd* fds, nfds_t nfds,
                        const struct timespec* timeout_ts,
                        const sigset_t* sigmask) {
    return __rin_ppoll_impl(fds, nfds, timeout_ts, sigmask);
}
#endif

/* ═══════════════════════════════════════════════════════════════
 * select関数 (poll.hに含める)
 * ═══════════════════════════════════════════════════════════════*/

/* `select()` writes the monotonic time remaining to a non-NULL mutable
 * `timeval` after the poll wait returns. `pselect()` takes a const `timespec`
 * and never writes it back. */
static inline int select(int nfds, fd_set* readfds, fd_set* writefds,
                         fd_set* exceptfds, struct timeval* timeout) {
    int64_t timeout_ns;
    struct timespec timeout_start;
    struct timeval timeout_snapshot;
    const struct timeval* original_timeout = NULL;

    if (nfds < 0 || nfds > FD_SETSIZE) {
        errno = EINVAL;
        return -1;
    }

    if (timeout) {
        if (__rin_poll_snapshot_timeval(
                timeout, &timeout_snapshot, &timeout_ns) != 0)
            return -1;
        original_timeout = &timeout_snapshot;
    } else {
        timeout_ns = -1;
    }
    if (original_timeout &&
        __rin_poll_timeval_to_ns(original_timeout, &timeout_ns) != 0)
        return -1;

    if (timeout && __rin_time_clock_gettime(
                       RIN_CLOCK_ID_MONOTONIC, &timeout_start) != 0) {
        return -1;
    }
    __rin_pthread_cancel_point_if_linked();
    int ret = __rin_select_syscall_once(
        nfds, readfds, writefds, exceptfds, timeout_ns, NULL);
    int poll_errno = errno;
    __rin_pthread_cancel_point_if_linked();
    if (timeout && __rin_select_update_timeval(
            timeout, original_timeout, &timeout_start) != 0) {
        return -1;
    }
    errno = poll_errno;
    return ret;
}

/* pselect - シグナルマスク付きselect */
static inline int pselect(int nfds, fd_set* readfds, fd_set* writefds,
                          fd_set* exceptfds, const struct timespec* timeout,
                          const sigset_t* sigmask) {
    if (nfds < 0 || nfds > FD_SETSIZE) {
        errno = EINVAL;
        return -1;
    }
    {
        struct timespec timeout_snapshot;
        int64_t timeout_ns;
        int result;
        if (__rin_poll_snapshot_timespec(
                timeout, &timeout_snapshot, &timeout_ns) != 0)
            return -1;
        __rin_pthread_cancel_point_if_linked();
        result = __rin_select_syscall_once(
            nfds, readfds, writefds, exceptfds, timeout_ns, sigmask);
        __rin_pthread_cancel_point_if_linked();
        return result;
    }
}

#ifdef __cplusplus
}
#endif

#endif /* _POLL_H */

#endif /* hosted Linux delegates to its native poll provider */
