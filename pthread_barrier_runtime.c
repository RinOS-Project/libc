/* SPDX-License-Identifier: MIT */
/* POSIX reusable barriers backed by private or process-shared futexes. */

#include "pthread.h"

#include <limits.h>

#define RIN_PTHREAD_BARRIER_MAGIC UINT32_C(0x52424152)

static int barrier_futex_operation(const pthread_barrier_t* barrier,
                                   int operation)
{
    return operation |
        (barrier->pshared == PTHREAD_PROCESS_SHARED ? 0
                                                     : FUTEX_PRIVATE_FLAG);
}

static int barrier_futex_wait(pthread_barrier_t* barrier,
                              volatile uint32_t* address, uint32_t expected)
{
    int saved_errno = errno;
    long result = syscall(SYS_futex, address,
                          barrier_futex_operation(barrier, FUTEX_WAIT),
                          expected, NULL, NULL, 0);
    int error = result == 0 ? 0 : (result == -1 ? errno : EIO);
    errno = saved_errno;
    return error;
}

static int barrier_futex_wake(pthread_barrier_t* barrier,
                              volatile uint32_t* address, int count)
{
    int saved_errno = errno;
    long result = syscall(SYS_futex, address,
                          barrier_futex_operation(barrier, FUTEX_WAKE),
                          count, NULL, NULL, 0);
    int error = result >= 0 ? 0 : (result == -1 ? errno : EIO);
    errno = saved_errno;
    return error;
}

static int barrier_lock(pthread_barrier_t* barrier)
{
    for (;;) {
        uint32_t expected = 0u;
        if (__atomic_compare_exchange_n(&barrier->lock, &expected, 1u, 0,
                                        __ATOMIC_ACQUIRE,
                                        __ATOMIC_RELAXED))
            return 0;

        __atomic_fetch_add(&barrier->lock_waiters, 1u, __ATOMIC_ACQ_REL);
        if (__atomic_load_n(&barrier->lock, __ATOMIC_ACQUIRE) != 0u) {
            int error = barrier_futex_wait(barrier, &barrier->lock, 1u);
            __atomic_fetch_sub(&barrier->lock_waiters, 1u, __ATOMIC_ACQ_REL);
            if (error != 0 && error != EAGAIN && error != EINTR)
                return error;
        } else {
            __atomic_fetch_sub(&barrier->lock_waiters, 1u, __ATOMIC_ACQ_REL);
        }
    }
}

static int barrier_unlock(pthread_barrier_t* barrier)
{
    __atomic_store_n(&barrier->lock, 0u, __ATOMIC_RELEASE);
    if (__atomic_load_n(&barrier->lock_waiters, __ATOMIC_ACQUIRE) != 0u)
        return barrier_futex_wake(barrier, &barrier->lock, 1);
    return 0;
}

int pthread_barrier_init(pthread_barrier_t* barrier,
                         const pthread_barrierattr_t* attr,
                         unsigned int count)
{
    uint32_t pshared = PTHREAD_PROCESS_PRIVATE;
    if (!barrier || count == 0u) return EINVAL;
    if (attr) {
        if (attr->pshared != PTHREAD_PROCESS_PRIVATE &&
            attr->pshared != PTHREAD_PROCESS_SHARED)
            return EINVAL;
#if defined(RIN_PTHREAD_PROCESS_SHARED_UNSUPPORTED)
        if (attr->pshared == PTHREAD_PROCESS_SHARED) return ENOTSUP;
#endif
        pshared = (uint32_t)attr->pshared;
    }
    barrier->count = count;
    __atomic_store_n(&barrier->waiting, 0u, __ATOMIC_RELAXED);
    __atomic_store_n(&barrier->generation, 0u, __ATOMIC_RELAXED);
    __atomic_store_n(&barrier->lock, 0u, __ATOMIC_RELAXED);
    __atomic_store_n(&barrier->lock_waiters, 0u, __ATOMIC_RELAXED);
    __atomic_store_n(&barrier->active_waiters, 0u, __ATOMIC_RELAXED);
    barrier->pshared = pshared;
    __atomic_store_n(&barrier->magic, RIN_PTHREAD_BARRIER_MAGIC,
                     __ATOMIC_RELEASE);
    return 0;
}

int pthread_barrier_destroy(pthread_barrier_t* barrier)
{
    int error;
    if (!barrier || __atomic_load_n(&barrier->magic, __ATOMIC_ACQUIRE) !=
                        RIN_PTHREAD_BARRIER_MAGIC)
        return EINVAL;
    error = barrier_lock(barrier);
    if (error != 0) return error;
    if (__atomic_load_n(&barrier->active_waiters, __ATOMIC_ACQUIRE) != 0u ||
        __atomic_load_n(&barrier->waiting, __ATOMIC_ACQUIRE) != 0u) {
        (void)barrier_unlock(barrier);
        return EBUSY;
    }
    __atomic_store_n(&barrier->magic, 0u, __ATOMIC_RELEASE);
    (void)barrier_unlock(barrier);
    return 0;
}

int pthread_barrier_wait(pthread_barrier_t* barrier)
{
    uint32_t generation;
    uint32_t waiting;
    int error;
    if (!barrier || __atomic_load_n(&barrier->magic, __ATOMIC_ACQUIRE) !=
                        RIN_PTHREAD_BARRIER_MAGIC ||
        barrier->count == 0u ||
        (barrier->pshared != PTHREAD_PROCESS_PRIVATE &&
         barrier->pshared != PTHREAD_PROCESS_SHARED))
        return EINVAL;

    error = barrier_lock(barrier);
    if (error != 0) return error;
    if (__atomic_load_n(&barrier->magic, __ATOMIC_ACQUIRE) !=
        RIN_PTHREAD_BARRIER_MAGIC) {
        (void)barrier_unlock(barrier);
        return EINVAL;
    }
    generation = __atomic_load_n(&barrier->generation, __ATOMIC_RELAXED);
    __atomic_fetch_add(&barrier->active_waiters, 1u, __ATOMIC_ACQ_REL);
    waiting = __atomic_add_fetch(&barrier->waiting, 1u, __ATOMIC_ACQ_REL);
    if (waiting == barrier->count) {
        __atomic_store_n(&barrier->waiting, 0u, __ATOMIC_RELAXED);
        __atomic_store_n(&barrier->generation, generation + 1u,
                         __ATOMIC_RELEASE);
        error = barrier_unlock(barrier);
        if (error == 0)
            error = barrier_futex_wake(barrier, &barrier->generation,
                                       INT_MAX);
        __atomic_fetch_sub(&barrier->active_waiters, 1u, __ATOMIC_ACQ_REL);
        return error == 0 ? PTHREAD_BARRIER_SERIAL_THREAD : error;
    }
    error = barrier_unlock(barrier);
    if (error != 0) {
        __atomic_fetch_sub(&barrier->active_waiters, 1u, __ATOMIC_ACQ_REL);
        return error;
    }

    while (__atomic_load_n(&barrier->generation, __ATOMIC_ACQUIRE) ==
           generation) {
        error = barrier_futex_wait(barrier, &barrier->generation, generation);
        if (error != 0 && error != EAGAIN && error != EINTR) {
            __atomic_fetch_sub(&barrier->active_waiters, 1u,
                               __ATOMIC_ACQ_REL);
            return error;
        }
    }
    __atomic_fetch_sub(&barrier->active_waiters, 1u, __ATOMIC_ACQ_REL);
    return 0;
}
