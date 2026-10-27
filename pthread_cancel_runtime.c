/* SPDX-License-Identifier: MIT */
/* Product-owned deferred POSIX cancellation state. */

#define RIN_PTHREAD_CANCEL_RUNTIME_IMPLEMENTATION 1
#include "pthread.h"

#define RIN_PTHREAD_CANCEL_RECORD_MAX 256u

typedef struct RinPthreadCancelRecord {
    pthread_t thread;
    volatile int pending;
    int state;
    int type;
    int active;
    int detached;
    void* exit_value;
    __rin_pthread_cleanup_buffer* cleanup_top;
} RinPthreadCancelRecord;

#define RIN_PTHREAD_CANCEL_SIGNAL __RIN_SIGCANCEL

#ifndef _RIN_PTHREAD_CANCEL_SELF
#define _RIN_PTHREAD_CANCEL_SELF() pthread_self()
#endif
#ifndef _RIN_PTHREAD_CANCEL_GETPID
#define _RIN_PTHREAD_CANCEL_GETPID() \
    _syscall0((uintptr_t)SYS_GETPID)
#endif
#ifndef _RIN_PTHREAD_CANCEL_WAKE
#define _RIN_PTHREAD_CANCEL_WAKE(process, thread, signum) \
    _syscall3((uintptr_t)SYS_tgkill, (uintptr_t)(process), \
              (uintptr_t)(thread), (uintptr_t)(signum))
#endif

static RinPthreadCancelRecord g_rin_pthread_cancel_records[
    RIN_PTHREAD_CANCEL_RECORD_MAX];
static volatile unsigned int g_rin_pthread_cancel_lock;

static void rin_pthread_cancel_lock(void) {
    while (__sync_lock_test_and_set(&g_rin_pthread_cancel_lock, 1u))
        (void)sched_yield();
}

static void rin_pthread_cancel_unlock(void) {
    __sync_lock_release(&g_rin_pthread_cancel_lock);
}

static RinPthreadCancelRecord* rin_pthread_cancel_find_locked(
    pthread_t thread) {
    unsigned int index;
    for (index = 0u; index < RIN_PTHREAD_CANCEL_RECORD_MAX; ++index) {
        if (g_rin_pthread_cancel_records[index].thread == thread)
            return &g_rin_pthread_cancel_records[index];
    }
    return NULL;
}

static RinPthreadCancelRecord* rin_pthread_cancel_slot_locked(
    pthread_t thread) {
    RinPthreadCancelRecord* free_slot = NULL;
    unsigned int index;
    for (index = 0u; index < RIN_PTHREAD_CANCEL_RECORD_MAX; ++index) {
        RinPthreadCancelRecord* record = &g_rin_pthread_cancel_records[index];
        if (record->thread == thread) return record;
        if (!record->thread && !free_slot) free_slot = record;
    }
    return free_slot;
}

int rin_pthread_cancel_register(pthread_t thread) {
    RinPthreadCancelRecord* record;
    if (thread == 0u) return EINVAL;
    rin_pthread_cancel_lock();
    record = rin_pthread_cancel_slot_locked(thread);
    if (!record) {
        rin_pthread_cancel_unlock();
        return EAGAIN;
    }
    if (record->thread != thread || !record->active) {
        record->thread = thread;
        record->pending = 0;
        record->state = PTHREAD_CANCEL_ENABLE;
        record->type = PTHREAD_CANCEL_DEFERRED;
        record->active = 1;
        record->detached = 0;
        record->exit_value = NULL;
        record->cleanup_top = NULL;
    }
    rin_pthread_cancel_unlock();
    return 0;
}

void rin_pthread_cancel_set_detached(pthread_t thread) {
    RinPthreadCancelRecord* record;
    if (thread == 0u) return;
    rin_pthread_cancel_lock();
    record = rin_pthread_cancel_find_locked(thread);
    if (record) {
        record->detached = 1;
        if (!record->active) {
            record->thread = 0u;
            record->pending = 0;
            record->exit_value = NULL;
            record->cleanup_top = NULL;
        }
    }
    rin_pthread_cancel_unlock();
}

void rin_pthread_cancel_exit(pthread_t thread, void* value) {
    RinPthreadCancelRecord* record;
    if (thread == 0u) return;
    rin_pthread_cancel_lock();
    record = rin_pthread_cancel_find_locked(thread);
    if (record) {
        record->active = 0;
        record->pending = 0;
        record->exit_value = value;
        record->cleanup_top = NULL;
        if (record->detached) {
            record->thread = 0u;
            record->exit_value = NULL;
        }
    }
    rin_pthread_cancel_unlock();
}

void rin_pthread_cancel_take_exit(pthread_t thread, void** value) {
    RinPthreadCancelRecord* record;
    if (value) *value = NULL;
    if (thread == 0u) return;
    rin_pthread_cancel_lock();
    record = rin_pthread_cancel_find_locked(thread);
    if (record) {
        if (value) *value = record->exit_value;
        record->thread = 0u;
        record->pending = 0;
        record->active = 0;
        record->detached = 0;
        record->exit_value = NULL;
        record->cleanup_top = NULL;
    }
    rin_pthread_cancel_unlock();
}

int pthread_setcancelstate(int state, int* oldstate) {
    pthread_t self = _RIN_PTHREAD_CANCEL_SELF();
    RinPthreadCancelRecord* record;
    if (state != PTHREAD_CANCEL_ENABLE && state != PTHREAD_CANCEL_DISABLE)
        return EINVAL;
    if (rin_pthread_cancel_register(self) != 0) return EAGAIN;
    rin_pthread_cancel_lock();
    record = rin_pthread_cancel_find_locked(self);
    if (!record) {
        rin_pthread_cancel_unlock();
        return EAGAIN;
    }
    if (oldstate) *oldstate = record->state;
    record->state = state;
    rin_pthread_cancel_unlock();
    if (state == PTHREAD_CANCEL_ENABLE) pthread_testcancel();
    return 0;
}

int pthread_setcanceltype(int type, int* oldtype) {
    pthread_t self = _RIN_PTHREAD_CANCEL_SELF();
    RinPthreadCancelRecord* record;
    if (type != PTHREAD_CANCEL_DEFERRED) return ENOTSUP;
    if (rin_pthread_cancel_register(self) != 0) return EAGAIN;
    rin_pthread_cancel_lock();
    record = rin_pthread_cancel_find_locked(self);
    if (!record) {
        rin_pthread_cancel_unlock();
        return EAGAIN;
    }
    if (oldtype) *oldtype = record->type;
    record->type = type;
    rin_pthread_cancel_unlock();
    return 0;
}

int pthread_cancel(pthread_t thread) {
    RinPthreadCancelRecord* record;
    int should_wake = 0;
    int needs_registration = 0;
    int saved_errno = errno;
    if (thread == 0u) return ESRCH;
    rin_pthread_cancel_lock();
    record = rin_pthread_cancel_find_locked(thread);
    if (!record) {
        needs_registration = 1;
    } else if (!record->active) {
        rin_pthread_cancel_unlock();
        return ESRCH;
    }
    rin_pthread_cancel_unlock();

    /* The initial process thread does not pass through pthread_create().
     * Admit a live, otherwise-unregistered thread before recording a request
     * so it can be cancelled by another pthread. */
    if (needs_registration) {
        int alive = pthread_kill(thread, 0);
        if (alive != 0) {
            errno = saved_errno;
            return alive == ESRCH ? ESRCH : alive;
        }
        if (rin_pthread_cancel_register(thread) != 0) {
            errno = saved_errno;
            return EAGAIN;
        }
    }

    rin_pthread_cancel_lock();
    record = rin_pthread_cancel_find_locked(thread);
    if (!record || !record->active) {
        rin_pthread_cancel_unlock();
        errno = saved_errno;
        return ESRCH;
    }
    if (!__atomic_load_n(&record->pending, __ATOMIC_ACQUIRE))
        __atomic_store_n(&record->pending, 1, __ATOMIC_RELEASE);
    should_wake = record->state == PTHREAD_CANCEL_ENABLE &&
        __atomic_load_n(&record->pending, __ATOMIC_ACQUIRE);
    rin_pthread_cancel_unlock();
    if (should_wake) {
        intptr_t process = _RIN_PTHREAD_CANCEL_GETPID();
        intptr_t wake_result;
        if (process <= 0) {
            errno = saved_errno;
            return _rin_pthread_error_from_result(process);
        }
        wake_result = _RIN_PTHREAD_CANCEL_WAKE(
            process, thread, RIN_PTHREAD_CANCEL_SIGNAL);
        if (wake_result < 0) {
            int wake_error = _rin_pthread_error_from_result(wake_result);
            rin_pthread_cancel_lock();
            record = rin_pthread_cancel_find_locked(thread);
            if (record && !record->active) wake_error = 0;
            rin_pthread_cancel_unlock();
            errno = saved_errno;
            return wake_error;
        }
    }
    errno = saved_errno;
    return 0;
}

int rin_pthread_cancel_pending_enabled(pthread_t thread) {
    RinPthreadCancelRecord* record;
    int pending = 0;
    if (thread == 0u) return 0;
    rin_pthread_cancel_lock();
    record = rin_pthread_cancel_find_locked(thread);
    if (record && record->active && record->state == PTHREAD_CANCEL_ENABLE &&
        __atomic_load_n(&record->pending, __ATOMIC_ACQUIRE))
        pending = 1;
    rin_pthread_cancel_unlock();
    return pending;
}

void __rin_pthread_cancel_point(void) {
    if (rin_pthread_cancel_pending_enabled(_RIN_PTHREAD_CANCEL_SELF()))
        pthread_testcancel();
}

void __rin_pthread_cleanup_push_node(__rin_pthread_cleanup_buffer* buffer,
                                     void (*routine)(void*), void* argument) {
    RinPthreadCancelRecord* record;
    pthread_t self = _RIN_PTHREAD_CANCEL_SELF();
    if (!buffer || !routine || rin_pthread_cancel_register(self) != 0) return;
    buffer->routine = routine;
    buffer->argument = argument;
    buffer->active = 1;
    rin_pthread_cancel_lock();
    record = rin_pthread_cancel_find_locked(self);
    if (record && record->active) {
        buffer->next = record->cleanup_top;
        record->cleanup_top = buffer;
    } else {
        buffer->next = NULL;
        buffer->active = 0;
    }
    rin_pthread_cancel_unlock();
}

void __rin_pthread_cleanup_pop_node(int execute) {
    RinPthreadCancelRecord* record;
    __rin_pthread_cleanup_buffer* buffer = NULL;
    pthread_t self = _RIN_PTHREAD_CANCEL_SELF();
    rin_pthread_cancel_lock();
    record = rin_pthread_cancel_find_locked(self);
    if (record && record->cleanup_top) {
        buffer = record->cleanup_top;
        record->cleanup_top = buffer->next;
        buffer->next = NULL;
        buffer->active = 0;
    }
    rin_pthread_cancel_unlock();
    if (execute && buffer && buffer->routine)
        buffer->routine(buffer->argument);
}

void rin_pthread_cancel_run_cleanups(pthread_t thread) {
    for (;;) {
        RinPthreadCancelRecord* record;
        __rin_pthread_cleanup_buffer* buffer;
        rin_pthread_cancel_lock();
        record = rin_pthread_cancel_find_locked(thread);
        buffer = record ? record->cleanup_top : NULL;
        if (buffer) {
            record->cleanup_top = buffer->next;
            buffer->next = NULL;
            buffer->active = 0;
        }
        rin_pthread_cancel_unlock();
        if (!buffer) break;
        if (buffer->routine) buffer->routine(buffer->argument);
    }
}

static int rin_pthread_cancel_take_pending(pthread_t thread,
                                          int require_enabled) {
    RinPthreadCancelRecord* record;
    int cancel = 0;
    if (thread == 0u) return 0;
    rin_pthread_cancel_lock();
    record = rin_pthread_cancel_find_locked(thread);
    if (record && record->active &&
        (!require_enabled || record->state == PTHREAD_CANCEL_ENABLE) &&
        __atomic_load_n(&record->pending, __ATOMIC_ACQUIRE)) {
        __atomic_store_n(&record->pending, 0, __ATOMIC_RELEASE);
        cancel = 1;
    }
    rin_pthread_cancel_unlock();
    return cancel;
}

static int rin_pthread_cancel_is_pending(pthread_t thread) {
    return rin_pthread_cancel_take_pending(thread, 1);
}

void pthread_testcancel(void) {
    if (rin_pthread_cancel_is_pending(_RIN_PTHREAD_CANCEL_SELF()))
        pthread_exit(PTHREAD_CANCELED);
}

void __rin_pthread_cancel_point_cleanup(
    void (*cleanup)(void*), void* argument) {
    pthread_t self = _RIN_PTHREAD_CANCEL_SELF();
    int old_state = PTHREAD_CANCEL_ENABLE;
    if (pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &old_state) != 0) {
        pthread_testcancel();
        return;
    }
    if (old_state != PTHREAD_CANCEL_ENABLE) {
        (void)pthread_setcancelstate(old_state, NULL);
        return;
    }

    if (rin_pthread_cancel_take_pending(self, 0)) {
        if (cleanup) cleanup(argument);
        (void)pthread_setcancelstate(old_state, NULL);
        pthread_exit(PTHREAD_CANCELED);
    }

    (void)pthread_setcancelstate(old_state, NULL);
    pthread_testcancel();
}
