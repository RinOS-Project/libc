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

#include "stddef.h"
#include "stdint.h"
#include "errno.h"
#include "limits.h"
#include "time.h"
#include "sys/types.h"
#include "sys/syscall.h"
#include "signal.h"
#include "stdlib.h"
#include "internal/pthread_cancel_point.h"

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

static inline intptr_t __rin_poll_result(intptr_t result) {
    result = __rin_syscall_posixize(result);
    if (result < 0) {
        if (result != -1) errno = EIO;
        return -1;
    }
    return result;
}

static inline void __rin_poll_clear_revents(struct pollfd* fds,
                                             nfds_t nfds) {
    if (!fds) return;
    for (nfds_t index = 0u; index < nfds; ++index)
        fds[index].revents = 0;
}

static inline void __rin_select_clear_sets(fd_set* readfds,
                                            fd_set* writefds,
                                            fd_set* exceptfds) {
    if (readfds) FD_ZERO(readfds);
    if (writefds) FD_ZERO(writefds);
    if (exceptfds) FD_ZERO(exceptfds);
}

#define RIN_SELECT_LOCAL_POLLFD_COUNT 8u

static inline int __rin_select_build_pollfds(
    int nfds, const fd_set* readfds, const fd_set* writefds,
    const fd_set* exceptfds, struct pollfd* local_pollfds,
    nfds_t local_capacity, struct pollfd** pollfds_out,
    nfds_t* poll_count_out)
{
    struct pollfd* pollfds;
    nfds_t count = 0u;
    nfds_t capacity = local_capacity;

    if (!pollfds_out || !poll_count_out) {
        errno = EINVAL;
        return -1;
    }
    *pollfds_out = local_pollfds;
    *poll_count_out = 0u;
    pollfds = local_pollfds;
    for (int fd = 0; fd < nfds; ++fd) {
        short events = 0;
        if (readfds && FD_ISSET(fd, readfds)) events |= POLLIN;
        if (writefds && FD_ISSET(fd, writefds)) events |= POLLOUT;
        if (exceptfds && FD_ISSET(fd, exceptfds)) events |= POLLPRI;
        if (!events) continue;

        if (count == capacity) {
            nfds_t next_capacity;
            struct pollfd* grown;
            if (capacity == 0u) {
                next_capacity = 1u;
            } else if (capacity > UINT_MAX / 2u) {
                next_capacity = (nfds_t)nfds;
            } else {
                next_capacity = capacity * 2u;
                if (next_capacity > (nfds_t)nfds)
                    next_capacity = (nfds_t)nfds;
            }
            if (next_capacity <= capacity
#if SIZE_MAX <= UINT32_MAX
                || (uint64_t)next_capacity *
                       (uint64_t)sizeof(*pollfds) > (uint64_t)SIZE_MAX
#endif
                ) {
                if (pollfds != local_pollfds) free(pollfds);
                errno = EOVERFLOW;
                return -1;
            }
            if (pollfds == local_pollfds) {
                grown = (struct pollfd*)malloc(
                    (size_t)next_capacity * sizeof(*pollfds));
                if (grown)
                    for (nfds_t i = 0u; i < count; ++i)
                        grown[i] = local_pollfds[i];
            } else {
                grown = (struct pollfd*)realloc(
                    pollfds, (size_t)next_capacity * sizeof(*pollfds));
            }
            if (!grown) {
                if (pollfds != local_pollfds) free(pollfds);
                errno = ENOMEM;
                return -1;
            }
            pollfds = grown;
            capacity = next_capacity;
        }
        pollfds[count].fd = fd;
        pollfds[count].events = events;
        pollfds[count].revents = 0;
        ++count;
    }
    *pollfds_out = pollfds;
    *poll_count_out = count;
    return 0;
}

static inline void __rin_select_release_pollfds(
    struct pollfd* pollfds, const struct pollfd* local_pollfds) {
    if (pollfds && pollfds != local_pollfds) free(pollfds);
}

static inline int __rin_select_unpack_pollfds(
    const struct pollfd* pollfds, nfds_t poll_count,
    fd_set* readfds, fd_set* writefds, fd_set* exceptfds) {
    int ready_count = 0;

    for (nfds_t i = 0u; i < poll_count; ++i) {
        if ((pollfds[i].revents & POLLNVAL) != 0) {
            errno = EBADF;
            return -1;
        }
    }

    __rin_select_clear_sets(readfds, writefds, exceptfds);
    for (nfds_t i = 0u; i < poll_count; ++i) {
        int fd_ready = 0;
        if (readfds && (pollfds[i].events & POLLIN) != 0 &&
            (pollfds[i].revents & (POLLIN | POLLHUP | POLLERR)) != 0) {
            FD_SET(pollfds[i].fd, readfds);
            fd_ready = 1;
        }
        if (writefds && (pollfds[i].events & POLLOUT) != 0 &&
            (pollfds[i].revents & (POLLOUT | POLLERR)) != 0) {
            FD_SET(pollfds[i].fd, writefds);
            fd_ready = 1;
        }
        if (exceptfds && (pollfds[i].events & POLLPRI) != 0 &&
            (pollfds[i].revents & POLLPRI) != 0) {
            FD_SET(pollfds[i].fd, exceptfds);
            fd_ready = 1;
        }
        if (fd_ready) ++ready_count;
    }
    return ready_count;
}

static inline int __rin_ppoll_syscall_once(
    struct pollfd* fds, nfds_t nfds, int timeout_ms,
    const sigset_t* sigmask) {
    intptr_t result = __rin_poll_result(_RIN_POLL_SYSCALL5(
        SYS_PPOLL, fds, nfds, timeout_ms, sigmask, sizeof(sigset_t)));
    if (result < 0) {
        __rin_poll_clear_revents(fds, nfds);
        return -1;
    }
    if (result > (intptr_t)INT_MAX) {
        errno = EOVERFLOW;
        __rin_poll_clear_revents(fds, nfds);
        return -1;
    }
    return (int)result;
}

static inline int __rin_poll_syscall_without_mask_change(
    struct pollfd* fds, nfds_t nfds, int timeout_ms) {
    return __rin_ppoll_syscall_once(fds, nfds, timeout_ms, NULL);
}

static inline int __rin_poll_timespec_to_ms(const struct timespec* timeout,
                                             int* timeout_ms) {
    long long milliseconds;

    if (!timeout_ms) {
        errno = EINVAL;
        return -1;
    }
    if (!timeout) {
        *timeout_ms = -1;
        return 0;
    }
    if (timeout->tv_sec < 0 || timeout->tv_nsec < 0 ||
        timeout->tv_nsec >= 1000000000L) {
        errno = EINVAL;
        return -1;
    }
    if (timeout->tv_sec > (time_t)(INT_MAX / 1000)) {
        errno = EOVERFLOW;
        return -1;
    }

    milliseconds = (long long)timeout->tv_sec * 1000LL;
    milliseconds += ((long long)timeout->tv_nsec + 999999LL) / 1000000LL;
    if (milliseconds > INT_MAX) {
        errno = EOVERFLOW;
        return -1;
    }
    *timeout_ms = (int)milliseconds;
    return 0;
}

static inline int __rin_poll_timeval_to_ms(const struct timeval* timeout,
                                            int* timeout_ms) {
    long long milliseconds;

    if (!timeout_ms) {
        errno = EINVAL;
        return -1;
    }
    if (!timeout) {
        *timeout_ms = -1;
        return 0;
    }
    if (timeout->tv_sec < 0 || timeout->tv_usec < 0 ||
        timeout->tv_usec >= 1000000L) {
        errno = EINVAL;
        return -1;
    }
    if (timeout->tv_sec > (time_t)(INT_MAX / 1000)) {
        errno = EOVERFLOW;
        return -1;
    }

    milliseconds = (long long)timeout->tv_sec * 1000LL;
    milliseconds += ((long long)timeout->tv_usec + 999LL) / 1000LL;
    if (milliseconds > INT_MAX) {
        errno = EOVERFLOW;
        return -1;
    }
    *timeout_ms = (int)milliseconds;
    return 0;
}

/* Linux updates select()'s timeval with the unspent portion of the original
 * timeout. Measure only the poll wait, using CLOCK_MONOTONIC so wall-clock
 * adjustments cannot extend or shorten it. */
static inline int __rin_select_update_timeval(
        struct timeval* timeout, const struct timespec* start) {
    const uint64_t nanoseconds_per_second = UINT64_C(1000000000);
    struct timespec now;
    uint64_t requested_ns;
    uint64_t elapsed_seconds;
    uint64_t elapsed_ns;
    uint64_t remaining_ns;
    int saved_errno;

    if (!timeout || !start) return 0;
    saved_errno = errno;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) return -1;
    if (start->tv_sec < 0 || start->tv_nsec < 0 ||
        start->tv_nsec >= 1000000000L || now.tv_sec < 0 ||
        now.tv_nsec < 0 || now.tv_nsec >= 1000000000L) {
        errno = EIO;
        return -1;
    }

    requested_ns = (uint64_t)timeout->tv_sec * nanoseconds_per_second +
                   (uint64_t)timeout->tv_usec * UINT64_C(1000);
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
    timeout->tv_sec = (time_t)(remaining_ns / nanoseconds_per_second);
    timeout->tv_usec = (long)((remaining_ns % nanoseconds_per_second) /
                              UINT64_C(1000));
    errno = saved_errno;
    return 0;
}

/* poll - ファイルディスクリプタを監視
 * 戻り値: イベントが発生したfdの数、0=タイムアウト、-1=エラー
 */
static inline int poll(struct pollfd* fds, nfds_t nfds, int timeout) {
    __rin_pthread_cancel_point_if_linked();
    if (nfds > RIN_POLL_MAX_FDS || (fds == NULL && nfds != 0u)) {
        errno = fds == NULL && nfds != 0u ? EFAULT : EINVAL;
        return -1;
    }
    if (timeout < 0) timeout = -1;
    {
        int result = __rin_poll_syscall_without_mask_change(
            fds, nfds, timeout);
        __rin_pthread_cancel_point_if_linked();
        return result;
    }
}

/* ppoll - シグナルマスク付きpoll */
static inline int ppoll(struct pollfd* fds, nfds_t nfds,
                        const struct timespec* timeout_ts,
                        const sigset_t* sigmask) {
    int timeout;
    int result;
    __rin_pthread_cancel_point_if_linked();

    if (nfds > RIN_POLL_MAX_FDS || (fds == NULL && nfds != 0u)) {
        errno = fds == NULL && nfds != 0u ? EFAULT : EINVAL;
        return -1;
    }
    if (__rin_poll_timespec_to_ms(timeout_ts, &timeout) != 0) return -1;
    result = !sigmask
        ? __rin_poll_syscall_without_mask_change(fds, nfds, timeout)
        : __rin_ppoll_syscall_once(fds, nfds, timeout, sigmask);
    __rin_pthread_cancel_point_if_linked();
    return result;
}

/* ═══════════════════════════════════════════════════════════════
 * select関数 (poll.hに含める)
 * ═══════════════════════════════════════════════════════════════*/

/* `select()` writes the monotonic time remaining to a non-NULL mutable
 * `timeval` after the poll wait returns. `pselect()` takes a const `timespec`
 * and never writes it back. */
static inline int select(int nfds, fd_set* readfds, fd_set* writefds,
                         fd_set* exceptfds, struct timeval* timeout) {
    int timeout_ms;
    struct pollfd local_pollfds[RIN_SELECT_LOCAL_POLLFD_COUNT];
    struct pollfd* pfds = NULL;
    nfds_t npfds = 0u;
    struct timespec timeout_start;

    if (nfds < 0 || nfds > FD_SETSIZE) {
        errno = EINVAL;
        return -1;
    }

    if (__rin_poll_timeval_to_ms(timeout, &timeout_ms) != 0) return -1;

    if (__rin_select_build_pollfds(nfds, readfds, writefds, exceptfds,
                                   local_pollfds,
                                   RIN_SELECT_LOCAL_POLLFD_COUNT,
                                   &pfds, &npfds) != 0)
        return -1;

    if (npfds == 0) {
        int ret;
        int poll_errno;
        if (timeout && clock_gettime(CLOCK_MONOTONIC, &timeout_start) != 0) {
            __rin_select_release_pollfds(pfds, local_pollfds);
            return -1;
        }
        ret = poll(NULL, 0, timeout_ms);
        poll_errno = errno;
        if (timeout && __rin_select_update_timeval(
                timeout, &timeout_start) != 0) {
            __rin_select_release_pollfds(pfds, local_pollfds);
            return -1;
        }
        errno = poll_errno;
        /* No descriptor below nfds was requested, so a successful select
         * must clear bits that were set only at or above nfds as well. */
        if (ret >= 0) {
            (void)__rin_select_unpack_pollfds(
                pfds, npfds, readfds, writefds, exceptfds);
        }
        __rin_select_release_pollfds(pfds, local_pollfds);
        return ret;
    }

    if (timeout && clock_gettime(CLOCK_MONOTONIC, &timeout_start) != 0) {
        __rin_select_release_pollfds(pfds, local_pollfds);
        return -1;
    }
    int ret = poll(pfds, npfds, timeout_ms);
    int poll_errno = errno;
    if (timeout && __rin_select_update_timeval(timeout, &timeout_start) != 0) {
        __rin_select_release_pollfds(pfds, local_pollfds);
        return -1;
    }
    errno = poll_errno;
    if (ret < 0) {
        __rin_select_release_pollfds(pfds, local_pollfds);
        return ret;
    }
    int ready_count = __rin_select_unpack_pollfds(
        pfds, npfds, readfds, writefds, exceptfds);
    if (ready_count < 0) {
        __rin_select_release_pollfds(pfds, local_pollfds);
        return -1;
    }

    __rin_select_release_pollfds(pfds, local_pollfds);
    return ready_count;
}

/* pselect - シグナルマスク付きselect */
static inline int pselect(int nfds, fd_set* readfds, fd_set* writefds,
                          fd_set* exceptfds, const struct timespec* timeout,
                          const sigset_t* sigmask) {
    int timeout_ms;

    if (nfds < 0 || nfds > FD_SETSIZE) {
        errno = EINVAL;
        return -1;
    }
    if (__rin_poll_timespec_to_ms(timeout, &timeout_ms) != 0) return -1;
    if (sigmask) {
        struct pollfd local_pollfds[RIN_SELECT_LOCAL_POLLFD_COUNT];
        struct pollfd* pfds = NULL;
        nfds_t npfds = 0u;
        int ret;
        if (__rin_select_build_pollfds(nfds, readfds, writefds, exceptfds,
                                       local_pollfds,
                                       RIN_SELECT_LOCAL_POLLFD_COUNT,
                                       &pfds, &npfds) != 0)
            return -1;
        ret = ppoll(pfds, npfds, timeout, sigmask);
        if (ret < 0) {
            __rin_select_release_pollfds(pfds, local_pollfds);
            return ret;
        }
        int ready_count = __rin_select_unpack_pollfds(
            pfds, npfds, readfds, writefds, exceptfds);
        if (ready_count < 0) {
            __rin_select_release_pollfds(pfds, local_pollfds);
            return -1;
        }
        __rin_select_release_pollfds(pfds, local_pollfds);
        return ready_count;
    }

    struct timeval tv;
    struct timeval* tvp = NULL;

    if (timeout) {
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        tvp = &tv;
    }

    return select(nfds, readfds, writefds, exceptfds, tvp);
}

#ifdef __cplusplus
}
#endif

#endif /* _POLL_H */

#endif /* hosted Linux delegates to its native poll provider */
