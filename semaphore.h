/* SPDX-License-Identifier: MIT */
/*
 * RinOS libc - semaphore.h
 * POSIX semaphore
 */

#ifndef _SEMAPHORE_H
#define _SEMAPHORE_H

#include "stddef.h"
#include "stdint.h"
#include "limits.h"
#include "errno.h"
#include "internal/pthread_cancel_point.h"
#include "sys/types.h"
#include "time.h"
#include "sys/syscall.h"
#include "linux/futex.h"
#include <rin/ipc/posix_semaphore_abi.h>

#ifndef _RIN_SEMAPHORE_FUTEX
#define _RIN_SEMAPHORE_FUTEX(address, operation, value, timeout) \
    syscall(SYS_futex, address, operation, value, timeout, NULL, 0)
#endif

#ifndef _RIN_SEMAPHORE_FUTEX_BITSET
#define _RIN_SEMAPHORE_FUTEX_BITSET(address, operation, value, timeout, bitset) \
    syscall(SYS_futex, address, operation, value, timeout, NULL, bitset)
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

#if defined(__cplusplus)
static_assert(sizeof(sem_t) == 8u, "sem_t shared ABI drift");
static_assert(offsetof(sem_t, waiters) == sizeof(int),
              "sem_t waiter offset drift");
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(sem_t) == 8u, "sem_t shared ABI drift");
_Static_assert(offsetof(sem_t, waiters) == sizeof(int),
               "sem_t waiter offset drift");
#endif

/* Preserve sem_t's two-word ABI while storing whether its futex key is
 * process-shared in the high bit of the waiter count. */
#define RIN_SEM_PSHARED_FLAG RIN_POSIX_SEMAPHORE_WAITERS_SHARED_FLAG
#define RIN_SEM_WAITER_MASK  RIN_POSIX_SEMAPHORE_WAITERS_COUNT_MASK

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

/* Cancellation after a successful wake leaves sem->value untouched.  Drop
 * this waiter's registration and pass the available token's wake to another
 * sleeper so a positive value cannot strand the remaining waiters. */
static inline void rin_sem_cancel_waiter_cleanup(void* argument) {
    sem_t* sem = (sem_t*)argument;
    int saved_errno = errno;
    uint32_t state;
    if (!sem) return;
    rin_sem_waiter_remove(sem);
    state = rin_sem_state_load(sem);
    if ((state & RIN_SEM_WAITER_MASK) != 0u &&
        __atomic_load_n(&sem->value, __ATOMIC_ACQUIRE) > 0) {
        (void)_RIN_SEMAPHORE_FUTEX(
            &sem->value, rin_sem_futex_operation(sem, FUTEX_WAKE), 1, NULL);
    }
    errno = saved_errno;
}

#define SEM_FAILED ((sem_t*)0)

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
    __rin_pthread_cancel_point_if_linked();
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
        
        /* Keep the waiter registered until cancellation cleanup has had a
         * chance to hand off a token made available by this wake. */
        __rin_pthread_cancel_point_cleanup_if_linked(
            rin_sem_cancel_waiter_cleanup, sem);
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

/* POSIX absolute-clock semaphore wait.  Named and unnamed semaphores share
 * this futex path; the waiter state selects a private or shared futex key. */
static inline int __rin_sem_clockwait_impl(
    sem_t* sem, clockid_t clock_id, const struct timespec* abstime) {
    __rin_pthread_cancel_point_if_linked();
    if (!sem || (clock_id != CLOCK_REALTIME &&
                 clock_id != CLOCK_MONOTONIC)) {
        errno = EINVAL;
        return -1;
    }

    for (;;) {
        int val = __atomic_load_n(&sem->value, __ATOMIC_RELAXED);
        if (val > 0) {
            if (__atomic_compare_exchange_n(&sem->value, &val, val - 1, 0,
                                            __ATOMIC_ACQUIRE,
                                            __ATOMIC_RELAXED))
                return 0;
            continue;
        }

        /* As with sem_timedwait(), an available token succeeds without
         * inspecting the caller's timeout object. */
        if (!abstime || abstime->tv_nsec < 0 ||
            abstime->tv_nsec >= 1000000000L) {
            errno = EINVAL;
            return -1;
        }
#if !defined(RIN_TIME_ABI64)
        if (clock_id == CLOCK_REALTIME && abstime->tv_sec < 0) {
            errno = EINVAL;
            return -1;
        }
#endif
        if (clock_id == CLOCK_MONOTONIC && abstime->tv_sec < 0) {
            if (sem_trywait(sem) == 0) return 0;
            errno = ETIMEDOUT;
            return -1;
        }

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

        long ret = _RIN_SEMAPHORE_FUTEX_BITSET(
            &sem->value,
            rin_sem_futex_operation(
                sem, FUTEX_WAIT_BITSET | FUTEX_CLOCK_REALTIME),
            0, abstime, FUTEX_BITSET_MATCH_ANY);
        __rin_pthread_cancel_point_cleanup_if_linked(
            rin_sem_cancel_waiter_cleanup, sem);
        rin_sem_waiter_remove(sem);

        if (ret < 0) {
            int err = errno;
            if (err == ETIMEDOUT) {
                /* A post can race with the timeout wake; take a token if it
                 * is already available before reporting the deadline. */
                if (sem_trywait(sem) == 0) return 0;
                errno = ETIMEDOUT;
                return -1;
            }
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
    int posted_value;
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
            posted_value = value + 1;
            break;
        }
    }
    
    /* Optimization: Only wake kernel if there are waiters */
    uint32_t state = rin_sem_state_load(sem);
    if ((state & RIN_SEM_WAITER_MASK) > 0u) {
        long wake_result = _RIN_SEMAPHORE_FUTEX(
            &sem->value, rin_sem_futex_operation(sem, FUTEX_WAKE), 1, NULL);
        if (wake_result < 0) {
            int error_number = errno;
            int expected = posted_value;
            /* Restore the token when no waiter or concurrent poster observed
             * it. If another thread already consumed/advanced the value, keep
             * that ownership change intact while still surfacing wake failure. */
            (void)__atomic_compare_exchange_n(&sem->value, &expected,
                                              posted_value - 1, 0,
                                              __ATOMIC_RELEASE,
                                              __ATOMIC_RELAXED);
            errno = error_number > 0 && error_number <= 4095
                        ? error_number : EIO;
            return -1;
        }
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
 * 名前付きセマフォはkernelのshared-memory objectが所有し、プロセス間で
 * 同じsem_t stateを共有する。sem_open()のvarargsはO_CREATのときmode_tと
 * 初期値をこの順で受け取る。mode_tのpermissionはcurrent-session namespace
 * 内では適用されない。
 */
sem_t* sem_open(const char* name, int oflag, ...);
int sem_close(sem_t* sem);
int sem_unlink(const char* name);

#ifdef __cplusplus
}
#endif

#endif /* _SEMAPHORE_H */
