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

/* Preserve sem_t's two-word ABI while storing whether its futex key is
 * process-shared in the high bit of the waiter count. */
#define RIN_SEM_PSHARED_FLAG UINT32_C(0x80000000)
#define RIN_SEM_WAITER_MASK  UINT32_C(0x7fffffff)

static inline uint32_t rin_sem_state_load(const sem_t* sem) {
    return __atomic_load_n((const uint32_t*)&sem->waiters, __ATOMIC_ACQUIRE);
}

static inline int rin_sem_is_shared(const sem_t* sem) {
    return (rin_sem_state_load(sem) & RIN_SEM_PSHARED_FLAG) != 0u;
}

static inline int rin_sem_waiter_add(sem_t* sem) {
    uint32_t state = rin_sem_state_load(sem);
    for (;;) {
        uint32_t count = state & RIN_SEM_WAITER_MASK;
        uint32_t next;
        if (count == RIN_SEM_WAITER_MASK) return -1;
        next = (state & RIN_SEM_PSHARED_FLAG) | (count + 1u);
        if (__atomic_compare_exchange_n((uint32_t*)&sem->waiters, &state,
                                        next, 0, __ATOMIC_SEQ_CST,
                                        __ATOMIC_ACQUIRE))
            return 0;
    }
}

static inline void rin_sem_waiter_remove(sem_t* sem) {
    uint32_t state = rin_sem_state_load(sem);
    for (;;) {
        uint32_t count = state & RIN_SEM_WAITER_MASK;
        uint32_t next;
        if (count == 0u) return;
        next = (state & RIN_SEM_PSHARED_FLAG) | (count - 1u);
        if (__atomic_compare_exchange_n((uint32_t*)&sem->waiters, &state,
                                        next, 0, __ATOMIC_SEQ_CST,
                                        __ATOMIC_ACQUIRE))
            return;
    }
}

static inline int rin_sem_futex_operation(const sem_t* sem, int operation) {
    return operation | (rin_sem_is_shared(sem) ? 0 : FUTEX_PRIVATE_FLAG);
}

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
    if (value > SEM_VALUE_MAX) {
        errno = EINVAL;
        return -1;
    }
    sem->value = (int)value;
    sem->waiters = (int)(pshared ? RIN_SEM_PSHARED_FLAG : 0u);
    return 0;
}

/* 無名セマフォの破棄 */
static inline int sem_destroy(sem_t* sem) {
    if (!sem) {
        errno = EINVAL;
        return -1;
    }
    if ((rin_sem_state_load(sem) & RIN_SEM_WAITER_MASK) != 0u) {
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
        if (rin_sem_waiter_add(sem) != 0) {
            errno = EAGAIN;
            return -1;
        }
        
        /* Double-check value after incrementing waiters (handles race with sem_post) */
        val = __atomic_load_n(&sem->value, __ATOMIC_RELAXED);
        if (val > 0) {
            /* Retry acquire */
            if (__atomic_compare_exchange_n(&sem->value, &val, val - 1, 0, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
                /* Acquired! Undo waiters increment and return */
                rin_sem_waiter_remove(sem);
                return 0;
            }
            /* CAS failed (someone else stole it), decrement waiters and retry loop */
            rin_sem_waiter_remove(sem);
            continue;
        }

        /* Sleep: wait for value to be 0 */
        /* If sem->value becomes != 0 before blocking, futex returns EAGAIN immediately */
        long ret = _RIN_SEMAPHORE_FUTEX(
            &sem->value, rin_sem_futex_operation(sem, FUTEX_WAIT), 0, NULL);
        
        /* Woke up (or error) - decrement waiters */
        rin_sem_waiter_remove(sem);

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

        if (rin_sem_waiter_add(sem) != 0) {
            errno = EAGAIN;
            return -1;
        }

        val = __atomic_load_n(&sem->value, __ATOMIC_RELAXED);
        if (val > 0) {
            if (__atomic_compare_exchange_n(&sem->value, &val, val - 1, 0, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
                rin_sem_waiter_remove(sem);
                return 0;
            }
            rin_sem_waiter_remove(sem);
            continue;
        }

        long ret = _RIN_SEMAPHORE_FUTEX(
            &sem->value, rin_sem_futex_operation(sem, FUTEX_WAIT), 0, &rel);
        rin_sem_waiter_remove(sem);

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
    uint32_t state = rin_sem_state_load(sem);
    if ((state & RIN_SEM_WAITER_MASK) > 0u) {
        syscall(SYS_futex, &sem->value,
                rin_sem_futex_operation(sem, FUTEX_WAKE),
                1, NULL, NULL, 0);
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
