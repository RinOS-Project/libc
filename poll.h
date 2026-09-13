/* SPDX-License-Identifier: MIT */
/*
 * RinOS libc - poll.h
 * I/O多重化
 */

#ifndef _POLL_H
#define _POLL_H

#include "stddef.h"
#include "errno.h"
#include "limits.h"
#include "time.h"
#include "sys/types.h"
#include "sys/syscall.h"

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
typedef unsigned long sigset_t;
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

/* A poll request is copied/validated by the kernel before it can block.  Keep
 * a finite userspace bound as well so a malformed nfds value cannot turn into
 * an unbounded scan or an overflowing byte count in a hosted shim. */
#ifndef RIN_POLL_MAX_FDS
#define RIN_POLL_MAX_FDS 65536u
#endif

/* ═══════════════════════════════════════════════════════════════
 * poll関数
 * ═══════════════════════════════════════════════════════════════*/

#ifndef _RIN_POLL_SYSCALL3
#define _RIN_POLL_SYSCALL3(number, argument1, argument2, argument3) \
    _syscall3((uintptr_t)(number), (uintptr_t)(argument1), \
              (uintptr_t)(argument2), (uintptr_t)(argument3))
#endif
#ifndef _RIN_POLL_SYSCALL1
#define _RIN_POLL_SYSCALL1(number, argument1) \
    _syscall1((uintptr_t)(number), (uintptr_t)(argument1))
#endif
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

static inline int __rin_poll_syscall_once(struct pollfd* fds, nfds_t nfds, int timeout) {
    intptr_t result = __rin_poll_result(_RIN_POLL_SYSCALL3(
        SYS_POLL, fds, nfds, timeout));
    if (result < 0) return -1;
    if (result > (intptr_t)INT_MAX) {
        errno = EOVERFLOW;
        return -1;
    }
    return (int)result;
}

static inline int __rin_ppoll_syscall_once(
    struct pollfd* fds, nfds_t nfds, int timeout_ms,
    const sigset_t* sigmask) {
    intptr_t result = __rin_poll_result(_RIN_POLL_SYSCALL5(
        SYS_PPOLL, fds, nfds, timeout_ms, sigmask, sizeof(sigset_t)));
    if (result < 0) return -1;
    if (result > (intptr_t)INT_MAX) {
        errno = EOVERFLOW;
        return -1;
    }
    return (int)result;
}

static inline long long __rin_poll_now_ms(void) {
    struct timespec now;
    long long seconds;
    long long milliseconds;
    unsigned long long seconds_unsigned;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
        return -1;
    }
    if (now.tv_sec < 0 || now.tv_nsec < 0 || now.tv_nsec >= 1000000000L) {
        errno = EOVERFLOW;
        return -1;
    }
    seconds_unsigned = (unsigned long long)now.tv_sec;
    if (seconds_unsigned > (unsigned long long)(LLONG_MAX / 1000LL)) {
        errno = EOVERFLOW;
        return -1;
    }
    seconds = (long long)now.tv_sec;
    milliseconds = seconds * 1000LL;
    if (milliseconds > LLONG_MAX -
            (long long)(now.tv_nsec / 1000000L)) {
        errno = EOVERFLOW;
        return -1;
    }
    return milliseconds + (long long)(now.tv_nsec / 1000000L);
}

static inline void __rin_poll_sleep_ms(uintptr_t ms) {
    (void)_RIN_POLL_SYSCALL1(SYS_SLEEP, ms);
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

/* poll - ファイルディスクリプタを監視
 * 戻り値: イベントが発生したfdの数、0=タイムアウト、-1=エラー
 */
static inline int poll(struct pollfd* fds, nfds_t nfds, int timeout) {
    if (nfds > RIN_POLL_MAX_FDS || (fds == NULL && nfds != 0u)) {
        errno = fds == NULL && nfds != 0u ? EFAULT : EINVAL;
        return -1;
    }
    if (timeout == 0) {
        return __rin_poll_syscall_once(fds, nfds, 0);
    }

    long long deadline_ms = -1;
    if (timeout > 0) {
        long long now_ms = __rin_poll_now_ms();
        if (now_ms < 0) {
            return __rin_poll_syscall_once(fds, nfds, timeout);
        }
        if (now_ms > LLONG_MAX - (long long)timeout) {
            /* Keep the finite timeout finite without signed overflow.  A
             * monotonic source reaching this boundary is already outside the
             * representable API range; treating the deadline as the largest
             * representable value preserves bounded progress. */
            deadline_ms = LLONG_MAX;
        } else {
            deadline_ms = now_ms + (long long)timeout;
        }
    }

    for (;;) {
        int rc = __rin_poll_syscall_once(fds, nfds, 0);
        if (rc != 0) {
            return rc;
        }

        if (timeout > 0) {
            long long now_ms = __rin_poll_now_ms();
            if (now_ms < 0 || now_ms >= deadline_ms) {
                return 0;
            }

            long long remaining_ms = deadline_ms - now_ms;
            uintptr_t sleep_ms = 10u;
            if (remaining_ms < (long long)sleep_ms) {
                sleep_ms = (uintptr_t)remaining_ms;
            }
            if (sleep_ms == 0) {
                sleep_ms = 1;
            }
            __rin_poll_sleep_ms(sleep_ms);
            continue;
        }

        __rin_poll_sleep_ms(1);
    }
}

/* ppoll - シグナルマスク付きpoll */
static inline int ppoll(struct pollfd* fds, nfds_t nfds,
                        const struct timespec* timeout_ts,
                        const sigset_t* sigmask) {
    int timeout;

    if (nfds > RIN_POLL_MAX_FDS || (fds == NULL && nfds != 0u)) {
        errno = fds == NULL && nfds != 0u ? EFAULT : EINVAL;
        return -1;
    }
    if (__rin_poll_timespec_to_ms(timeout_ts, &timeout) != 0) return -1;
    if (!sigmask) return poll(fds, nfds, timeout);
    return __rin_ppoll_syscall_once(fds, nfds, timeout, sigmask);
}

/* ═══════════════════════════════════════════════════════════════
 * select関数 (poll.hに含める)
 * ═══════════════════════════════════════════════════════════════*/

static inline int select(int nfds, fd_set* readfds, fd_set* writefds,
                         fd_set* exceptfds, struct timeval* timeout) {
    int timeout_ms;

    if (nfds < 0 || nfds > FD_SETSIZE) {
        errno = EINVAL;
        return -1;
    }

    if (__rin_poll_timeval_to_ms(timeout, &timeout_ms) != 0) return -1;

    /* pollfd配列を構築 */
    struct pollfd pfds[FD_SETSIZE];
    int npfds = 0;

    for (int fd = 0; fd < nfds; fd++) {
        short events = 0;

        if (readfds && FD_ISSET(fd, readfds)) {
            events |= POLLIN;
        }
        if (writefds && FD_ISSET(fd, writefds)) {
            events |= POLLOUT;
        }
        if (exceptfds && FD_ISSET(fd, exceptfds)) {
            events |= POLLPRI;
        }

        if (events) {
            pfds[npfds].fd = fd;
            pfds[npfds].events = events;
            pfds[npfds].revents = 0;
            npfds++;
        }
    }

    if (npfds == 0) {
        return poll(NULL, 0, timeout_ms);
    }

    int ret = poll(pfds, (nfds_t)npfds, timeout_ms);
    if (ret < 0) return ret;

    /* fd_setをクリアして結果を設定 */
    if (readfds) FD_ZERO(readfds);
    if (writefds) FD_ZERO(writefds);
    if (exceptfds) FD_ZERO(exceptfds);

    for (int i = 0; i < npfds; i++) {
        if (pfds[i].revents & (POLLIN | POLLHUP | POLLERR)) {
            if (readfds) {
                FD_SET(pfds[i].fd, readfds);
            }
        }
        if (pfds[i].revents & POLLOUT) {
            if (writefds) {
                FD_SET(pfds[i].fd, writefds);
            }
        }
        if (pfds[i].revents & (POLLPRI | POLLNVAL)) {
            if (exceptfds) {
                FD_SET(pfds[i].fd, exceptfds);
            }
        }
    }

    return ret;
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
        struct pollfd pfds[FD_SETSIZE];
        int npfds = 0;
        for (int fd = 0; fd < nfds; ++fd) {
            short events = 0;
            if (readfds && FD_ISSET(fd, readfds)) events |= POLLIN;
            if (writefds && FD_ISSET(fd, writefds)) events |= POLLOUT;
            if (exceptfds && FD_ISSET(fd, exceptfds)) events |= POLLPRI;
            if (events) {
                pfds[npfds].fd = fd;
                pfds[npfds].events = events;
                pfds[npfds].revents = 0;
                ++npfds;
            }
        }
        int ret = ppoll(pfds, (nfds_t)npfds, timeout, sigmask);
        if (ret < 0) return ret;
        if (readfds) FD_ZERO(readfds);
        if (writefds) FD_ZERO(writefds);
        if (exceptfds) FD_ZERO(exceptfds);
        for (int i = 0; i < npfds; ++i) {
            if (pfds[i].revents & (POLLIN | POLLHUP | POLLERR)) {
                if (readfds) FD_SET(pfds[i].fd, readfds);
            }
            if (pfds[i].revents & POLLOUT) {
                if (writefds) FD_SET(pfds[i].fd, writefds);
            }
            if (pfds[i].revents & (POLLPRI | POLLNVAL)) {
                if (exceptfds) FD_SET(pfds[i].fd, exceptfds);
            }
        }
        return ret;
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
