/* SPDX-License-Identifier: MIT */
/*
 * RinOS libc - semaphore.h
 * POSIX semaphore
 */

#ifndef _SEMAPHORE_H
#define _SEMAPHORE_H

#include "stddef.h"
#include "stdint.h"
#include "errno.h"
#include "sys/types.h"
#include "time.h"
#include "sys/syscall.h"
#include "linux/futex.h"
#include "rin_thread_timeout_policy.h"
#include <rin/ipc/posix_semaphore_abi.h>

#ifndef _RIN_SEMAPHORE_FUTEX
#define _RIN_SEMAPHORE_FUTEX(address, operation, value, timeout) \
    syscall(SYS_futex, address, operation, value, timeout, NULL, 0)
#endif

#ifndef _RIN_SEMAPHORE_CLOCK_GETTIME
#define _RIN_SEMAPHORE_CLOCK_GETTIME(clock_id, output) \
    clock_gettime(clock_id, output)
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════
 * セマフォ型
 * ═══════════════════════════════════════════════════════════════*/

typedef struct {
    int value;
    int waiters;
} sem_t;

#define SEM_FAILED ((sem_t*)0)

#ifndef SEM_VALUE_MAX
#define SEM_VALUE_MAX 0x7fffffffU
#endif

/* ═══════════════════════════════════════════════════════════════
 * セマフォ操作
 * ═══════════════════════════════════════════════════════════════*/

/* 無名セマフォの初期化 */
static inline int sem_init(sem_t* sem, int pshared, unsigned int value) {
    if (!sem) {
        errno = EINVAL;
        return -1;
    }
    /* FUTEX_WAIT/FUTEX_WAKE without FUTEX_PRIVATE_FLAG remains process
     * shared when the caller places sem_t in shared memory. */
    (void)pshared;
    if (value > SEM_VALUE_MAX) {
        errno = EINVAL;
        return -1;
    }
    sem->value = (int)value;
    sem->waiters = 0;
    return 0;
}

/* 無名セマフォの破棄 */
static inline int sem_destroy(sem_t* sem) {
    if (!sem) {
        errno = EINVAL;
        return -1;
    }
    if (__atomic_load_n(&sem->waiters, __ATOMIC_ACQUIRE) != 0) {
        errno = EBUSY;
        return -1;
    }
    return 0;
}

/* セマフォのデクリメント (P操作/wait) */
static inline int sem_wait(sem_t* sem) {
    if (!sem) {
        errno = EINVAL;
        return -1;
    }
    while (1) {
        /* Fast path: value > 0 */
        int val = __atomic_load_n(&sem->value, __ATOMIC_RELAXED);
        if (val > 0) {
            /* Try to acquire (strong CAS handles spurious failures internally) */
            if (__atomic_compare_exchange_n(&sem->value, &val, val - 1, 0, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
                return 0;
            }
            /* CAS failure: value changed (likely consumed by another thread), retry immediately */
            continue;
        }

        /* Slow path: prepare to sleep */
        /* Increment waiters count BEFORE sleep to ensure signal is not missed */
        __atomic_fetch_add(&sem->waiters, 1, __ATOMIC_SEQ_CST);
        
        /* Double-check value after incrementing waiters (handles race with sem_post) */
        val = __atomic_load_n(&sem->value, __ATOMIC_RELAXED);
        if (val > 0) {
            /* Retry acquire */
            if (__atomic_compare_exchange_n(&sem->value, &val, val - 1, 0, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
                /* Acquired! Undo waiters increment and return */
                __atomic_fetch_sub(&sem->waiters, 1, __ATOMIC_SEQ_CST);
                return 0;
            }
            /* CAS failed (someone else stole it), decrement waiters and retry loop */
            __atomic_fetch_sub(&sem->waiters, 1, __ATOMIC_SEQ_CST);
            continue;
        }

        /* Sleep: wait for value to be 0 */
        /* If sem->value becomes != 0 before blocking, futex returns EAGAIN immediately */
        long ret = _RIN_SEMAPHORE_FUTEX(
            &sem->value, FUTEX_WAIT, 0, NULL);
        
        /* Woke up (or error) - decrement waiters */
        __atomic_fetch_sub(&sem->waiters, 1, __ATOMIC_SEQ_CST);

        if (ret < 0 && errno != EAGAIN) {
            if (errno == 0) errno = EIO;
            return -1;
        }
        if (ret > 0) {
            errno = EIO;
            return -1;
        }
    }
}

/* セマフォの非ブロッキングデクリメント */
static inline int sem_trywait(sem_t* sem) {
    if (!sem) {
        errno = EINVAL;
        return -1;
    }
    while (1) {
        int val = __atomic_load_n(&sem->value, __ATOMIC_RELAXED);
        if (val > 0) {
            if (__atomic_compare_exchange_n(&sem->value, &val, val - 1, 0, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
                return 0;
            }
            /* CAS failed: retry (robust against contention) */
        } else {
            /* Value is 0: Resource unavailable */
            errno = EAGAIN;
            return -1;
        }
    }
}

/* セマフォのタイムアウト付きデクリメント */
static inline int sem_timedwait(sem_t* sem, const struct timespec* abstime) {
    if (!sem || !abstime) {
        errno = EINVAL;
        return -1;
    }

    while (1) {
        int val = __atomic_load_n(&sem->value, __ATOMIC_RELAXED);
        if (val > 0) {
            if (__atomic_compare_exchange_n(&sem->value, &val, val - 1, 0, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
                return 0;
            }
            continue;
        }

        struct timespec now;
        struct rin_thread_relative_timeout relative;
        enum rin_thread_timeout_outcome timeout_outcome;
        if (abstime->tv_nsec < 0 || abstime->tv_nsec >= 1000000000L) {
            errno = EINVAL;
            return -1;
        }
        if (_RIN_SEMAPHORE_CLOCK_GETTIME(CLOCK_REALTIME, &now) != 0) {
            if (errno == 0) errno = EIO;
            return -1;
        }
        timeout_outcome = rin_thread_relative_timeout(
            (long)abstime->tv_sec, abstime->tv_nsec,
            (long)now.tv_sec, now.tv_nsec, __LONG_MAX__, &relative);
        if (timeout_outcome == RIN_THREAD_TIMEOUT_INVALID) {
            errno = EIO;
            return -1;
        }
        if (timeout_outcome == RIN_THREAD_TIMEOUT_EXPIRED) {
            errno = ETIMEDOUT;
            return -1;
        }

        struct timespec rel;
        rel.tv_sec = (time_t)relative.seconds;
        rel.tv_nsec = relative.nanoseconds;

        __atomic_fetch_add(&sem->waiters, 1, __ATOMIC_SEQ_CST);

        val = __atomic_load_n(&sem->value, __ATOMIC_RELAXED);
        if (val > 0) {
            if (__atomic_compare_exchange_n(&sem->value, &val, val - 1, 0, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
                __atomic_fetch_sub(&sem->waiters, 1, __ATOMIC_SEQ_CST);
                return 0;
            }
            __atomic_fetch_sub(&sem->waiters, 1, __ATOMIC_SEQ_CST);
            continue;
        }

        long ret = _RIN_SEMAPHORE_FUTEX(
            &sem->value, FUTEX_WAIT, 0, &rel);
        __atomic_fetch_sub(&sem->waiters, 1, __ATOMIC_SEQ_CST);

        if (ret < 0) {
            int err = errno;
            if (err != EAGAIN) {
                if (err == 0) errno = EIO;
                return -1;
            }
        } else if (ret > 0) {
            errno = EIO;
            return -1;
        }
    }
}

/* セマフォのインクリメント (V操作/post) */
static inline int sem_post(sem_t* sem) {
    int value;
    if (!sem) {
        errno = EINVAL;
        return -1;
    }

    value = __atomic_load_n(&sem->value, __ATOMIC_RELAXED);
    for (;;) {
        if ((unsigned int)value >= SEM_VALUE_MAX) {
            errno = EOVERFLOW;
            return -1;
        }
        if (__atomic_compare_exchange_n(&sem->value, &value, value + 1, 0,
                                        __ATOMIC_RELEASE, __ATOMIC_RELAXED)) {
            break;
        }
    }
    
    /* Optimization: Only wake kernel if there are waiters */
    int w = __atomic_load_n(&sem->waiters, __ATOMIC_SEQ_CST);
    if (w > 0) {
        syscall(SYS_futex, &sem->value, FUTEX_WAKE, 1, NULL, NULL, 0);
    }
    return 0;
}

/* セマフォの値を取得 */
static inline int sem_getvalue(sem_t* sem, int* sval) {
    if (!sem || !sval) {
        errno = EINVAL;
        return -1;
    }
    *sval = __atomic_load_n(&sem->value, __ATOMIC_RELAXED);
    return 0;
}

/*
 * 名前付きセマフォは libc runtime の bounded process-local namespace が
 * 所有する。無名セマフォと違い、sem_open() の varargs は O_CREAT のとき
 * mode_t と初期値をこの順で受け取る。
 */
sem_t* sem_open(const char* name, int oflag, ...);
int sem_close(sem_t* sem);
int sem_unlink(const char* name);

#ifdef __cplusplus
}
#endif

#endif /* _SEMAPHORE_H */
