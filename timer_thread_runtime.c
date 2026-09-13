/* SPDX-License-Identifier: MIT */
/* Bounded SIGEV_THREAD bridge: kernel timer events are consumed by a private
 * signal and each callback runs on a detached libc thread. */
#define RIN_TIMER_THREAD_RUNTIME 1
#include "time.h"
#include "signal.h"
#include "pthread.h"

#define RIN_TIMER_THREAD_SLOTS 16u
#define RIN_TIMER_THREAD_JOBS 8u
typedef struct RinTimerThreadEntry {
    volatile sig_atomic_t active;
    timer_t timer_id;
    void (*function)(union sigval);
    union sigval value;
    unsigned int generation;
} RinTimerThreadEntry;
typedef struct RinTimerThreadJob {
    volatile sig_atomic_t active;
    void (*function)(union sigval);
    union sigval value;
} RinTimerThreadJob;
static RinTimerThreadEntry g_entries[RIN_TIMER_THREAD_SLOTS];
static RinTimerThreadJob g_jobs[RIN_TIMER_THREAD_JOBS];
static volatile sig_atomic_t g_entries_lock;
static volatile sig_atomic_t g_handler_installed;
static volatile sig_atomic_t g_dispatcher_started;
static volatile sig_atomic_t g_signal_pending;

#if defined(RIN_TIMER_THREAD_POLL)
#define RIN_TIMER_THREAD_POLL_EXTERNAL 1
#endif
#ifndef RIN_TIMER_THREAD_POLL
#define RIN_TIMER_THREAD_POLL(timer_id) \
    _rin_time_result(_RIN_TIME_TIMER_POLL(timer_id))
#endif
#if defined(RIN_TIMER_THREAD_INSTALL_HANDLER)
#define RIN_TIMER_THREAD_INSTALL_EXTERNAL 1
#endif
#ifndef RIN_TIMER_THREAD_INSTALL_HANDLER
#define RIN_TIMER_THREAD_INSTALL_HANDLER(action) \
    sigaction(RIN_TIMER_THREAD_SIGNAL, (action), (struct sigaction*)0)
#endif
#if defined(RIN_TIMER_THREAD_SLEEP)
#define RIN_TIMER_THREAD_SLEEP_EXTERNAL 1
#endif
#ifndef RIN_TIMER_THREAD_SLEEP
#define RIN_TIMER_THREAD_SLEEP(request) nanosleep((request), (struct timespec*)0)
#endif

#ifdef RIN_TIMER_THREAD_POLL_EXTERNAL
extern intptr_t RIN_TIMER_THREAD_POLL(timer_t timer_id);
#endif
#ifdef RIN_TIMER_THREAD_INSTALL_EXTERNAL
extern int RIN_TIMER_THREAD_INSTALL_HANDLER(const struct sigaction* action);
#endif
#ifdef RIN_TIMER_THREAD_SLEEP_EXTERNAL
extern int RIN_TIMER_THREAD_SLEEP(const struct timespec* request);
#endif

typedef struct RinTimerThreadSnapshot {
    unsigned int slot;
    timer_t timer_id;
    void (*function)(union sigval);
    union sigval value;
    unsigned int generation;
} RinTimerThreadSnapshot;

static void rin_timer_thread_lock(void) {
    while (__sync_lock_test_and_set(&g_entries_lock, 1) != 0) {
        /* Registration/unregistration is short and the signal handler never
         * takes this lock, so a bounded spin avoids another syscall here. */
    }
}

static void rin_timer_thread_unlock(void) {
    __sync_lock_release(&g_entries_lock);
}

static void* rin_timer_thread_job(void* argument) {
    RinTimerThreadJob* job = (RinTimerThreadJob*)argument;
    if (job && job->function) job->function(job->value);
    if (job) {
        job->function = (void (*)(union sigval))0;
        __atomic_store_n(&job->active, 0, __ATOMIC_RELEASE);
    }
    return (void*)0;
}

static void rin_timer_thread_signal(int signum) {
    (void)signum;
    /* POSIX signal handlers may only perform async-signal-safe work.  The
     * dispatcher performs timer polling and pthread operations later in
     * ordinary thread context.  A volatile sig_atomic_t assignment is the
     * portable wake-up primitive for the handler/dispatcher hand-off. */
    g_signal_pending = 1;
}

static int rin_timer_thread_snapshot_current(
    const RinTimerThreadSnapshot* snapshot) {
    RinTimerThreadEntry* entry;
    int current = 0;
    rin_timer_thread_lock();
    entry = snapshot->slot < RIN_TIMER_THREAD_SLOTS
        ? &g_entries[snapshot->slot] : (RinTimerThreadEntry*)0;
    if (entry && __atomic_load_n(&entry->active, __ATOMIC_ACQUIRE) == 1 &&
        entry->timer_id == snapshot->timer_id &&
        entry->generation == snapshot->generation &&
        entry->function == snapshot->function) {
        current = 1;
    }
    rin_timer_thread_unlock();
    return current;
}

static RinTimerThreadJob* rin_timer_thread_job_reserve(void) {
    RinTimerThreadJob* job = (RinTimerThreadJob*)0;
    for (unsigned int j = 0u; j < RIN_TIMER_THREAD_JOBS; ++j) {
        sig_atomic_t expected = 0;
        if (__atomic_compare_exchange_n(&g_jobs[j].active, &expected, 1,
                                         0, __ATOMIC_ACQ_REL,
                                         __ATOMIC_ACQUIRE)) {
            job = &g_jobs[j];
            break;
        }
    }
    return job;
}

static unsigned int rin_timer_thread_dispatch_once(void) {
    RinTimerThreadSnapshot snapshots[RIN_TIMER_THREAD_SLOTS];
    unsigned int snapshot_count = 0u;
    unsigned int dispatched = 0u;

    rin_timer_thread_lock();
    for (unsigned int i = 0u; i < RIN_TIMER_THREAD_SLOTS; ++i) {
        RinTimerThreadEntry* entry = &g_entries[i];
        if (__atomic_load_n(&entry->active, __ATOMIC_ACQUIRE) != 1)
            continue;
        snapshots[snapshot_count].slot = i;
        snapshots[snapshot_count].timer_id = entry->timer_id;
        snapshots[snapshot_count].function = entry->function;
        snapshots[snapshot_count].value = entry->value;
        snapshots[snapshot_count].generation = entry->generation;
        ++snapshot_count;
    }
    rin_timer_thread_unlock();

    for (unsigned int i = 0u; i < snapshot_count; ++i) {
        const RinTimerThreadSnapshot* snapshot = &snapshots[i];
        for (;;) {
            RinTimerThreadJob* job;
            pthread_t thread;
            intptr_t pending;

            if (!rin_timer_thread_snapshot_current(snapshot)) break;
            pending = RIN_TIMER_THREAD_POLL(snapshot->timer_id);
            if (pending <= 0) break;
            if (!rin_timer_thread_snapshot_current(snapshot)) break;

            job = rin_timer_thread_job_reserve();
            if (!job) break;
            job->function = snapshot->function;
            job->value = snapshot->value;
            if (pthread_create(&thread, (const pthread_attr_t*)0,
                               rin_timer_thread_job, job) != 0) {
                job->function = (void (*)(union sigval))0;
                __atomic_store_n(&job->active, 0, __ATOMIC_RELEASE);
                break;
            }
            /* A successful create transfers ownership of the job record to
             * the worker even when detach itself fails; clearing it here
             * would race the worker's first read. */
            if (pthread_detach(thread) != 0) break;
            ++dispatched;
        }
    }
    return dispatched;
}

static void* rin_timer_thread_dispatcher(void* argument) {
    (void)argument;
    for (;;) {
        (void)rin_timer_thread_dispatch_once();
        if (g_signal_pending != 0) {
            g_signal_pending = 0;
            continue;
        }
        {
            struct timespec delay;
            delay.tv_sec = 0;
            delay.tv_nsec = 1000000L;
            (void)RIN_TIMER_THREAD_SLEEP(&delay);
        }
    }
    return (void*)0;
}

static int rin_timer_thread_start_dispatcher(void) {
    sig_atomic_t expected = 0;
    pthread_t thread;
    if (!__atomic_compare_exchange_n(&g_dispatcher_started, &expected, 1,
                                      0, __ATOMIC_ACQ_REL,
                                      __ATOMIC_ACQUIRE)) {
        return 0;
    }
    if (pthread_create(&thread, (const pthread_attr_t*)0,
                       rin_timer_thread_dispatcher, (void*)0) != 0) {
        __atomic_store_n(&g_dispatcher_started, 0, __ATOMIC_RELEASE);
        return -1;
    }
    /* The dispatcher is process-lifetime state.  If detach fails it still
     * remains started; callers fail closed but later registrations may reuse
     * the already-running dispatcher. */
    (void)pthread_detach(thread);
    return 0;
}

int rin_timer_thread_register(timer_t timer_id,
                              void (*function)(union sigval),
                              union sigval value) {
    struct sigaction action;
    if (timer_id <= 0 || !function) return -1;
    if (!__atomic_load_n(&g_handler_installed, __ATOMIC_ACQUIRE)) {
        action.sa_handler = rin_timer_thread_signal;
        action.sa_mask = 0;
        action.sa_flags = 0;
        action.sa_restorer = (void (*)(void))0;
        if (RIN_TIMER_THREAD_INSTALL_HANDLER(&action) != 0)
            return -1;
        __atomic_store_n(&g_handler_installed, 1, __ATOMIC_RELEASE);
    }

    if (rin_timer_thread_start_dispatcher() != 0) return -1;

    rin_timer_thread_lock();
    for (unsigned int i = 0u; i < RIN_TIMER_THREAD_SLOTS; ++i) {
        if (__atomic_load_n(&g_entries[i].active, __ATOMIC_ACQUIRE) != 0)
            continue;
        g_entries[i].timer_id = timer_id;
        g_entries[i].function = function;
        g_entries[i].value = value;
        ++g_entries[i].generation;
        if (g_entries[i].generation == 0u) g_entries[i].generation = 1u;
        __atomic_store_n(&g_entries[i].active, 1, __ATOMIC_RELEASE);
        rin_timer_thread_unlock();
        return 0;
    }
    rin_timer_thread_unlock();
    return -1;
}

void rin_timer_thread_unregister(timer_t timer_id) {
    rin_timer_thread_lock();
    for (unsigned int i = 0u; i < RIN_TIMER_THREAD_SLOTS; ++i) {
        if (__atomic_load_n(&g_entries[i].active, __ATOMIC_ACQUIRE) != 0 &&
            g_entries[i].timer_id == timer_id) {
            __atomic_store_n(&g_entries[i].active, 0, __ATOMIC_RELEASE);
            g_entries[i].function = (void (*)(union sigval))0;
            g_entries[i].value.sival_ptr = (void*)0;
            rin_timer_thread_unlock();
            return;
        }
    }
    rin_timer_thread_unlock();
}

#ifdef RIN_TIMER_THREAD_RUNTIME_TESTING
void rin_timer_thread_runtime_test_signal(void) {
    rin_timer_thread_signal(RIN_TIMER_THREAD_SIGNAL);
}

unsigned int rin_timer_thread_runtime_test_dispatch_once(void) {
    return rin_timer_thread_dispatch_once();
}
#endif
