/* SPDX-License-Identifier: MIT */
/* C11 threads API implemented by the RinOS POSIX thread owner. */
#ifndef RIN_LIBC_THREADS_H
#define RIN_LIBC_THREADS_H

#include "errno.h"
#include "pthread.h"
#include "stdint.h"
#include "stdlib.h"
#include "time.h"

#ifdef __cplusplus
extern "C" {
#else
#ifndef thread_local
#define thread_local _Thread_local
#endif
#endif

#define thrd_success 0
#define thrd_busy 1
#define thrd_error 2
#define thrd_nomem 3
#define thrd_timedout 4

#define mtx_plain 1
#define mtx_timed 2
#define mtx_recursive 4

#define TSS_DTOR_ITERATIONS PTHREAD_DESTRUCTOR_ITERATIONS

typedef pthread_t thrd_t;
typedef pthread_mutex_t mtx_t;
typedef pthread_cond_t cnd_t;
typedef pthread_key_t tss_t;
typedef pthread_once_t once_flag;

#define ONCE_FLAG_INIT 0

typedef int (*thrd_start_t)(void*);
typedef void (*tss_dtor_t)(void*);

typedef struct __rin_c11_thread_start {
    thrd_start_t function;
    void* argument;
} __rin_c11_thread_start;

static inline void* __rin_c11_thread_entry(void* opaque)
{
    __rin_c11_thread_start* start = (__rin_c11_thread_start*)opaque;
    thrd_start_t function = start->function;
    void* argument = start->argument;
    free(start);
    return (void*)(intptr_t)function(argument);
}

static inline int thrd_create(thrd_t* thread, thrd_start_t function,
                              void* argument)
{
    __rin_c11_thread_start* start;
    pthread_t created;
    int error;

    if (!thread || !function) return thrd_error;
    start = (__rin_c11_thread_start*)malloc(sizeof(*start));
    if (!start) return thrd_nomem;
    start->function = function;
    start->argument = argument;
    error = pthread_create(&created, NULL, __rin_c11_thread_entry, start);
    if (error != 0) {
        free(start);
        return error == EAGAIN || error == ENOMEM ? thrd_nomem : thrd_error;
    }
    *thread = created;
    return thrd_success;
}

static inline thrd_t thrd_current(void)
{
    return pthread_self();
}

static inline int thrd_equal(thrd_t left, thrd_t right)
{
    return pthread_equal(left, right) != 0;
}

static inline int thrd_sleep(const struct timespec* duration,
                             struct timespec* remaining)
{
    if (nanosleep(duration, remaining) == 0) return 0;
    return -1;
}

static inline void thrd_yield(void)
{
    (void)sched_yield();
}

static inline int thrd_join(thrd_t thread, int* result)
{
    void* value = NULL;
    int error = pthread_join(thread, &value);
    if (error != 0) return thrd_error;
    if (result) *result = (int)(intptr_t)value;
    return thrd_success;
}

static inline int thrd_detach(thrd_t thread)
{
    return pthread_detach(thread) == 0 ? thrd_success : thrd_error;
}

static inline void thrd_exit(int result)
{
    pthread_exit((void*)(intptr_t)result);
}

static inline int mtx_init(mtx_t* mutex, int type)
{
    pthread_mutexattr_t attr;
    int error;
    if (!mutex || (type & ~(mtx_plain | mtx_timed | mtx_recursive)) != 0 ||
        (type & (mtx_plain | mtx_timed)) == 0 ||
        (type & (mtx_plain | mtx_timed)) == (mtx_plain | mtx_timed))
        return thrd_error;
    error = pthread_mutexattr_init(&attr);
    if (error != 0) return thrd_error;
    if ((type & mtx_recursive) != 0)
        error = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    if (error == 0) error = pthread_mutex_init(mutex, &attr);
    (void)pthread_mutexattr_destroy(&attr);
    return error == 0 ? thrd_success : thrd_error;
}

static inline int mtx_lock(mtx_t* mutex)
{
    return pthread_mutex_lock(mutex) == 0 ? thrd_success : thrd_error;
}

static inline int mtx_timedlock(mtx_t* mutex, const struct timespec* deadline)
{
    int error;
    if (!mutex || !deadline) return thrd_error;
    /* Both pthread owners use an absolute CLOCK_REALTIME wait. */
    error = pthread_mutex_timedlock(mutex, deadline);
    if (error == 0) return thrd_success;
    return error == ETIMEDOUT ? thrd_timedout : thrd_error;
}

static inline int mtx_trylock(mtx_t* mutex)
{
    int error = pthread_mutex_trylock(mutex);
    if (error == 0) return thrd_success;
    return error == EBUSY ? thrd_busy : thrd_error;
}

static inline int mtx_unlock(mtx_t* mutex)
{
    return pthread_mutex_unlock(mutex) == 0 ? thrd_success : thrd_error;
}

static inline void mtx_destroy(mtx_t* mutex)
{
    (void)pthread_mutex_destroy(mutex);
}

static inline int cnd_init(cnd_t* condition)
{
    return pthread_cond_init(condition, NULL) == 0 ? thrd_success : thrd_error;
}

static inline int cnd_signal(cnd_t* condition)
{
    return pthread_cond_signal(condition) == 0 ? thrd_success : thrd_error;
}

static inline int cnd_broadcast(cnd_t* condition)
{
    return pthread_cond_broadcast(condition) == 0 ? thrd_success : thrd_error;
}

static inline int cnd_wait(cnd_t* condition, mtx_t* mutex)
{
    return pthread_cond_wait(condition, mutex) == 0 ? thrd_success : thrd_error;
}

static inline int cnd_timedwait(cnd_t* condition, mtx_t* mutex,
                                const struct timespec* deadline)
{
    int error;
    if (!condition || !mutex || !deadline) return thrd_error;
    error = pthread_cond_timedwait(condition, mutex, deadline);
    if (error == 0) return thrd_success;
    return error == ETIMEDOUT ? thrd_timedout : thrd_error;
}

static inline void cnd_destroy(cnd_t* condition)
{
    (void)pthread_cond_destroy(condition);
}

static inline int tss_create(tss_t* key, tss_dtor_t destructor)
{
    int error;
    if (!key) return thrd_error;
    error = pthread_key_create(key, destructor);
    if (error == 0) return thrd_success;
    return error == ENOMEM ? thrd_nomem : thrd_error;
}

static inline void tss_delete(tss_t key)
{
    (void)pthread_key_delete(key);
}

static inline void* tss_get(tss_t key)
{
    return pthread_getspecific(key);
}

static inline int tss_set(tss_t key, void* value)
{
    return pthread_setspecific(key, value) == 0 ? thrd_success : thrd_error;
}

static inline void call_once(once_flag* flag, void (*function)(void))
{
    if (flag && function) (void)pthread_once(flag, function);
}

#ifdef __cplusplus
}
#endif

#endif /* RIN_LIBC_THREADS_H */
