/* SPDX-License-Identifier: MIT */
/* Bounded SIGEV_THREAD bridge: kernel timer events are consumed by a private
 * signal and each callback runs on a detached libc thread. */
#define RIN_TIMER_THREAD_RUNTIME 1
#include "time.h"
#include "signal.h"
#include "pthread.h"

#define RIN_TIMER_THREAD_SLOTS 16u
#define RIN_TIMER_THREAD_JOBS 8u
#define RIN_TIMER_THREAD_RETRY_NS 10000000L
typedef struct RinTimerThreadEntry {
    volatile sig_atomic_t active;
    timer_t timer_id;
    void (*function)(union sigval);
    union sigval value;
    unsigned int generation;
} RinTimerThreadEntry;
typedef struct RinTimerThreadJob {
    volatile sig_atomic_t active;
    unsigned int slot;
    timer_t timer_id;
    unsigned int generation;
    void (*function)(union sigval);
    union sigval value;
} RinTimerThreadJob;
static RinTimerThreadEntry g_entries[RIN_TIMER_THREAD_SLOTS];
static RinTimerThreadJob g_jobs[RIN_TIMER_THREAD_JOBS];
static volatile sig_atomic_t g_entries_lock;
static volatile sig_atomic_t g_handler_installed;
static volatile sig_atomic_t g_dispatcher_started;
static volatile sig_atomic_t g_signal_pending;
static volatile sig_atomic_t g_timer_poll_pending;
static volatile uint32_t g_dispatcher_event_generation;

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
#ifdef RIN_TIMER_THREAD_POLL_EXTERNAL
extern intptr_t RIN_TIMER_THREAD_POLL(timer_t timer_id);
#endif
#ifdef RIN_TIMER_THREAD_INSTALL_EXTERNAL
extern int RIN_TIMER_THREAD_INSTALL_HANDLER(const struct sigaction* action);
#endif
#ifdef RIN_TIMER_THREAD_RUNTIME_TESTING
extern intptr_t rin_timer_thread_runtime_test_futex_wait(
    volatile uint32_t* address, uint32_t expected,
    const struct timespec* timeout);
extern intptr_t rin_timer_thread_runtime_test_futex_wake(
    volatile uint32_t* address, unsigned int count);
#define RIN_TIMER_THREAD_FUTEX_WAIT(address, expected, timeout) \
    rin_timer_thread_runtime_test_futex_wait((address), (expected), (timeout))
#define RIN_TIMER_THREAD_FUTEX_WAKE(address, count) \
    rin_timer_thread_runtime_test_futex_wake((address), (count))
#else
#define RIN_TIMER_THREAD_FUTEX_WAIT(address, expected, timeout) \
    _RIN_TIME_FUTEX_WAIT((address), FUTEX_WAIT_PRIVATE, (expected), \
                         (timeout), FUTEX_BITSET_MATCH_ANY)
#define RIN_TIMER_THREAD_FUTEX_WAKE(address, count) \
    _syscall6((uintptr_t)SYS_futex, (uintptr_t)(address), \
              (uintptr_t)FUTEX_WAKE_PRIVATE, (uintptr_t)(count), \
              0u, 0u, 0u)
#endif

typedef struct RinTimerThreadSnapshot {
    unsigned int slot;
    timer_t timer_id;
    void (*function)(union sigval);
    union sigval value;
    unsigned int generation;
} RinTimerThreadSnapshot;

static void rin_timer_thread_publish_wake(void);

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
        job->timer_id = 0;
        job->slot = 0u;
        job->generation = 0u;
        __atomic_store_n(&job->active, 0, __ATOMIC_RELEASE);
        rin_timer_thread_publish_wake();
    }
    return (void*)0;
}

static void rin_timer_thread_signal(int signum) {
    (void)signum;
    /* Timer signals may be delivered to any process thread.  The futex wake
     * also releases the dispatcher when another thread ran this handler. */
    __atomic_store_n(&g_signal_pending, 1, __ATOMIC_RELEASE);
    rin_timer_thread_publish_wake();
}

static void rin_timer_thread_publish_wake(void) {
    __atomic_fetch_add(&g_dispatcher_event_generation, 1u, __ATOMIC_RELEASE);
    (void)RIN_TIMER_THREAD_FUTEX_WAKE(&g_dispatcher_event_generation, 1u);
}

static void rin_timer_thread_wait_for_signal(uint32_t observed) {
    struct timespec retry_delay;
    const struct timespec* timeout = (const struct timespec*)0;
    if (__atomic_load_n(&g_signal_pending, __ATOMIC_ACQUIRE) != 0)
        return;
    if (__atomic_load_n(&g_dispatcher_event_generation, __ATOMIC_ACQUIRE) !=
        observed)
        return;
    for (unsigned int i = 0u; i < RIN_TIMER_THREAD_JOBS; ++i) {
        if (__atomic_load_n(&g_jobs[i].active, __ATOMIC_ACQUIRE) == 1 &&
            g_jobs[i].function) {
            retry_delay.tv_sec = 0;
            retry_delay.tv_nsec = RIN_TIMER_THREAD_RETRY_NS;
            timeout = &retry_delay;
            break;
        }
    }
    (void)RIN_TIMER_THREAD_FUTEX_WAIT(&g_dispatcher_event_generation,
                                      observed, timeout);
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

static void rin_timer_thread_job_release(RinTimerThreadJob* job) {
    if (!job) return;
    job->function = (void (*)(union sigval))0;
    job->timer_id = 0;
    job->slot = 0u;
    job->generation = 0u;
    __atomic_store_n(&job->active, 0, __ATOMIC_RELEASE);
}

/* A consumed timer event stays in its bounded job slot until a worker can be
 * created.  Returning the slot to the pool after pthread_create() fails would
 * silently discard that one-shot notification. */
static unsigned int rin_timer_thread_launch_pending_jobs(void) {
    unsigned int dispatched = 0u;
    for (unsigned int index = 0u; index < RIN_TIMER_THREAD_JOBS; ++index) {
        RinTimerThreadJob* job = &g_jobs[index];
        RinTimerThreadSnapshot snapshot;
        pthread_t thread;
        sig_atomic_t expected = 1;
        if (__atomic_load_n(&job->active, __ATOMIC_ACQUIRE) != 1 ||
            !job->function)
            continue;
        snapshot.slot = job->slot;
        snapshot.timer_id = job->timer_id;
        snapshot.function = job->function;
        snapshot.value = job->value;
        snapshot.generation = job->generation;
        if (!rin_timer_thread_snapshot_current(&snapshot)) {
            rin_timer_thread_job_release(job);
            continue;
        }
        if (!__atomic_compare_exchange_n(&job->active, &expected, 2, 0,
                                         __ATOMIC_ACQ_REL,
                                         __ATOMIC_ACQUIRE))
            continue;
        if (pthread_create(&thread, (const pthread_attr_t*)0,
                           rin_timer_thread_job, job) != 0) {
            /* No worker owns the job after a failed create.  Keep the
             * callback and timer generation for the next dispatcher pass. */
            __atomic_store_n(&job->active, 1, __ATOMIC_RELEASE);
            continue;
        }
        /* A successful create transfers ownership of the job record to the
         * worker even when detach itself fails. */
        (void)pthread_detach(thread);
        ++dispatched;
    }
    return dispatched;
}

static unsigned int rin_timer_thread_dispatch_once(void) {
    RinTimerThreadSnapshot snapshots[RIN_TIMER_THREAD_SLOTS];
    unsigned int snapshot_count = 0u;
    unsigned int dispatched = 0u;
    int poll_pending;

    poll_pending =
        __atomic_exchange_n(&g_signal_pending, 0, __ATOMIC_ACQ_REL) != 0;
    if (__atomic_exchange_n(&g_timer_poll_pending, 0,
                            __ATOMIC_ACQ_REL) != 0)
        poll_pending = 1;
    dispatched += rin_timer_thread_launch_pending_jobs();
    if (!poll_pending) return dispatched;

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

    dispatched += rin_timer_thread_launch_pending_jobs();

    for (unsigned int i = 0u; i < snapshot_count; ++i) {
        const RinTimerThreadSnapshot* snapshot = &snapshots[i];
        for (;;) {
            RinTimerThreadJob* job;
            intptr_t pending;

            if (!rin_timer_thread_snapshot_current(snapshot)) break;
            /* Reserve capacity before TIMER_POLL consumes the one-shot
             * expiration.  If every worker slot is occupied, leave the
             * kernel notification pending for the next dispatcher pass. */
            job = rin_timer_thread_job_reserve();
            if (!job) {
                __atomic_store_n(&g_timer_poll_pending, 1, __ATOMIC_RELEASE);
                break;
            }
            pending = RIN_TIMER_THREAD_POLL(snapshot->timer_id);
            if (pending <= 0 ||
                !rin_timer_thread_snapshot_current(snapshot)) {
                rin_timer_thread_job_release(job);
                break;
            }
            job->slot = snapshot->slot;
            job->timer_id = snapshot->timer_id;
            job->generation = snapshot->generation;
            job->function = snapshot->function;
            job->value = snapshot->value;
            dispatched += rin_timer_thread_launch_pending_jobs();
            /* A create failure leaves this consumed notification in its
             * reserved slot.  Do not consume a second event for this timer
             * until that callback has a worker. */
            if (__atomic_load_n(&job->active, __ATOMIC_ACQUIRE) == 1)
                break;
        }
    }
    return dispatched;
}

static void* rin_timer_thread_dispatcher(void* argument) {
    (void)argument;
    for (;;) {
        uint32_t observed = __atomic_load_n(&g_dispatcher_event_generation,
                                             __ATOMIC_ACQUIRE);
        (void)rin_timer_thread_dispatch_once();
        rin_timer_thread_wait_for_signal(observed);
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
        rin_timer_thread_publish_wake();
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
            rin_timer_thread_publish_wake();
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

int rin_timer_thread_runtime_test_wait_for_signal(void) {
    uint32_t observed = __atomic_load_n(&g_dispatcher_event_generation,
                                         __ATOMIC_ACQUIRE);
    rin_timer_thread_wait_for_signal(observed);
    return 0;
}
#endif
