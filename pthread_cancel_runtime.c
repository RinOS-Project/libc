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
} RinPthreadCancelRecord;

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
    }
    rin_pthread_cancel_unlock();
}

int pthread_setcancelstate(int state, int* oldstate) {
    pthread_t self = pthread_self();
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
    return 0;
}

int pthread_setcanceltype(int type, int* oldtype) {
    pthread_t self = pthread_self();
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
    if (thread == 0u) return ESRCH;
    rin_pthread_cancel_lock();
    record = rin_pthread_cancel_find_locked(thread);
    if (!record || !record->active) {
        rin_pthread_cancel_unlock();
        return ESRCH;
    }
    __atomic_store_n(&record->pending, 1, __ATOMIC_RELEASE);
    rin_pthread_cancel_unlock();
    return 0;
}

static int rin_pthread_cancel_is_pending(pthread_t thread) {
    RinPthreadCancelRecord* record;
    int cancel = 0;
    if (thread == 0u) return 0;
    rin_pthread_cancel_lock();
    record = rin_pthread_cancel_find_locked(thread);
    if (record && record->active && record->state == PTHREAD_CANCEL_ENABLE &&
        __atomic_load_n(&record->pending, __ATOMIC_ACQUIRE)) {
        __atomic_store_n(&record->pending, 0, __ATOMIC_RELEASE);
        cancel = 1;
    }
    rin_pthread_cancel_unlock();
    return cancel;
}

void pthread_testcancel(void) {
    if (rin_pthread_cancel_is_pending(pthread_self()))
        pthread_exit(PTHREAD_CANCELED);
}
