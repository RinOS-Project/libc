/* SPDX-License-Identifier: MIT */
/* Product-owned POSIX cancellation state.
 *
 * Fast path: each thread caches its own record pointer in thread-local
 * storage, so a cancellation point is one TLS load plus a few atomic loads:
 * no global lock, no table scan, no yield.  Slow paths (first touch,
 * external pthread_cancel, join/detach/exit bookkeeping, fork) take a
 * sleepable recursive registry lock and use a shared hash index.
 *
 * Lifetime rules:
 * - A record is freed only when its owner thread can no longer run with a
 *   stale TLS pointer: the joiner takes the exit record after the target
 *   died, a detached thread clears its own TLS pointer before freeing its
 *   record on exit, set_detached frees only inactive records, and the fork
 *   child frees only other threads' records.
 * - pending/state/type/active are mutated with atomics so the lock-free
 *   self path, an external canceller, and (for async cancellation) a
 *   signal handler agree on one word each.
 * - cleanup_top is owner-only and lock-free; only the owning thread pushes,
 *   pops, or runs its own cleanups in production.
 * - The registry lock is recursive so a fork prepare handler running on the
 *   forking thread may call cancellation APIs while prepare holds the lock.
 *   The fork child resets the lock and rebuilds the index from the
 *   all-records list, which keeps a fork torn mid-insert dispatchable.
 */

#define RIN_PTHREAD_CANCEL_RUNTIME_IMPLEMENTATION 1
#include "pthread.h"
#include "stdlib.h"
#include "string.h"
#include "errno.h"

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
#ifndef _RIN_PTHREAD_CANCEL_FUTEX
#define _RIN_PTHREAD_CANCEL_FUTEX(address, operation, value, timeout) \
    syscall(SYS_futex, (address), (operation), (value), (timeout), NULL, 0)
#endif
#ifndef _RIN_PTHREAD_CANCEL_ALLOC
#define _RIN_PTHREAD_CANCEL_ALLOC(size) malloc(size)
#endif
#ifndef _RIN_PTHREAD_CANCEL_FREE
#define _RIN_PTHREAD_CANCEL_FREE(pointer) free(pointer)
#endif

typedef struct RinPthreadCancelRecord {
    pthread_t thread;
    int pending;
    int state;
    int type;
    int active;
    int detached;
    int async_defer;
    void* exit_value;
    __rin_pthread_cleanup_buffer* cleanup_top;
    struct RinPthreadCancelRecord* hash_next;
    struct RinPthreadCancelRecord* all_next;
} RinPthreadCancelRecord;

#define RIN_PTHREAD_CANCEL_HASH_INITIAL 64u

static RinPthreadCancelRecord* g_rin_pthread_cancel_static_buckets[
    RIN_PTHREAD_CANCEL_HASH_INITIAL];
static RinPthreadCancelRecord** g_rin_pthread_cancel_buckets =
    g_rin_pthread_cancel_static_buckets;
static size_t g_rin_pthread_cancel_bucket_mask =
    RIN_PTHREAD_CANCEL_HASH_INITIAL - 1u;
static size_t g_rin_pthread_cancel_count;
static int g_rin_pthread_cancel_buckets_dynamic;
static RinPthreadCancelRecord* g_rin_pthread_cancel_all;
static int g_rin_pthread_cancel_fork_hooked;

/* Registry lock: 0 free, 1 locked, 2 locked with waiters.  Recursive for
 * the owning thread; waiters sleep on the word with FUTEX_WAIT_PRIVATE. */
static volatile unsigned int g_rin_pthread_cancel_lock;
static pthread_t g_rin_pthread_cancel_lock_owner;
static unsigned int g_rin_pthread_cancel_lock_depth;

/* The self cache sits on pthread_once's dynamic scope (once uses cleanup
 * push, which registers the caller).  The Windows host-test toolchain
 * emulates __thread through a pthread_once call, and __declspec(thread)
 * is ignored there, so TLS here would re-enter RinOS's own pthread_once
 * forever in TUs that link this runtime.  A plain cell is safe on that
 * host-only path: every load verifies thread identity before use, only
 * the owning thread publishes its own record, and the owner clears the
 * cell on exit, so a foreign value just falls back to the slow path.
 * Production (ELF) toolchains use real TLS. */
#if defined(_WIN32)
static RinPthreadCancelRecord* t_rin_pthread_cancel_self;
#else
static __thread RinPthreadCancelRecord* t_rin_pthread_cancel_self;
#endif

static size_t rin_pthread_cancel_hash(pthread_t thread) {
    uintptr_t key = (uintptr_t)thread;
    key ^= key >> 16;
#if UINTPTR_MAX > 0xFFFFFFFFu
    key *= (uintptr_t)UINT64_C(0x9E3779B97F4A7C15);
#else
    key *= (uintptr_t)0x9E3779B9u;
#endif
    key ^= key >> 13;
    return (size_t)key;
}

static void rin_pthread_cancel_lock(pthread_t self) {
    unsigned int word;
    if (self != 0u &&
        __atomic_load_n(&g_rin_pthread_cancel_lock_owner,
                        __ATOMIC_ACQUIRE) == self &&
        __atomic_load_n(&g_rin_pthread_cancel_lock_depth,
                        __ATOMIC_ACQUIRE) != 0u) {
        __atomic_store_n(&g_rin_pthread_cancel_lock_depth,
                         g_rin_pthread_cancel_lock_depth + 1u,
                         __ATOMIC_RELAXED);
        return;
    }
    for (;;) {
        word = __atomic_load_n(&g_rin_pthread_cancel_lock,
                               __ATOMIC_ACQUIRE);
        if (word == 0u &&
            __atomic_compare_exchange_n(&g_rin_pthread_cancel_lock,
                                        &word, 1u, 0, __ATOMIC_ACQUIRE,
                                        __ATOMIC_RELAXED)) {
            __atomic_store_n(&g_rin_pthread_cancel_lock_owner, self,
                             __ATOMIC_RELAXED);
            __atomic_store_n(&g_rin_pthread_cancel_lock_depth, 1u,
                             __ATOMIC_RELAXED);
            return;
        }
        if (self != 0u &&
            __atomic_load_n(&g_rin_pthread_cancel_lock_owner,
                            __ATOMIC_ACQUIRE) == self &&
            __atomic_load_n(&g_rin_pthread_cancel_lock_depth,
                            __ATOMIC_ACQUIRE) != 0u) {
            __atomic_store_n(&g_rin_pthread_cancel_lock_depth,
                             g_rin_pthread_cancel_lock_depth + 1u,
                             __ATOMIC_RELAXED);
            return;
        }
        /* Mark waiters and sleep; any wake, steal, or spurious error just
         * re-polls the word above.  No yield-spin: a preempted owner does
         * not cost waiters CPU. */
        if (__atomic_exchange_n(&g_rin_pthread_cancel_lock, 2u,
                                __ATOMIC_ACQ_REL) == 0u) {
            __atomic_store_n(&g_rin_pthread_cancel_lock_owner, self,
                             __ATOMIC_RELAXED);
            __atomic_store_n(&g_rin_pthread_cancel_lock_depth, 1u,
                             __ATOMIC_RELAXED);
            return;
        }
        {
            int saved_errno = errno;
            (void)_RIN_PTHREAD_CANCEL_FUTEX(
                &g_rin_pthread_cancel_lock, FUTEX_WAIT_PRIVATE, 2u, NULL);
            errno = saved_errno;
        }
    }
}

static void rin_pthread_cancel_unlock(void) {
    unsigned int depth = g_rin_pthread_cancel_lock_depth;
    if (depth > 1u) {
        __atomic_store_n(&g_rin_pthread_cancel_lock_depth, depth - 1u,
                         __ATOMIC_RELAXED);
        return;
    }
    __atomic_store_n(&g_rin_pthread_cancel_lock_depth, 0u,
                     __ATOMIC_RELAXED);
    __atomic_store_n(&g_rin_pthread_cancel_lock_owner, (pthread_t)0,
                     __ATOMIC_RELAXED);
    if (__atomic_exchange_n(&g_rin_pthread_cancel_lock, 0u,
                            __ATOMIC_RELEASE) == 2u) {
        int saved_errno = errno;
        (void)_RIN_PTHREAD_CANCEL_FUTEX(
            &g_rin_pthread_cancel_lock, FUTEX_WAKE_PRIVATE, 1u, NULL);
        errno = saved_errno;
    }
}

static RinPthreadCancelRecord* rin_pthread_cancel_find_locked(
    pthread_t thread) {
    RinPthreadCancelRecord* record;
    if (thread == 0u) return NULL;
    record = g_rin_pthread_cancel_buckets[
        rin_pthread_cancel_hash(thread) & g_rin_pthread_cancel_bucket_mask];
    while (record) {
        if (record->thread == thread) return record;
        record = record->hash_next;
    }
    return NULL;
}

static void rin_pthread_cancel_bucket_insert_locked(
    RinPthreadCancelRecord* record) {
    size_t slot = rin_pthread_cancel_hash(record->thread) &
        g_rin_pthread_cancel_bucket_mask;
    record->hash_next = g_rin_pthread_cancel_buckets[slot];
    g_rin_pthread_cancel_buckets[slot] = record;
}

/* Double the bucket array when it fills.  Allocation failure keeps the old
 * array: lookups stay correct, only longer.  Registration itself never
 * fails here, only on record allocation. */
static void rin_pthread_cancel_maybe_grow_locked(void) {
    size_t capacity = g_rin_pthread_cancel_bucket_mask + 1u;
    size_t index;
    RinPthreadCancelRecord** grown;
    if (g_rin_pthread_cancel_count <= capacity * 2u) return;
    if (capacity > (size_t)-1 / (2u * sizeof(*grown))) return;
    grown = (RinPthreadCancelRecord**)_RIN_PTHREAD_CANCEL_ALLOC(
        capacity * 2u * sizeof(*grown));
    if (!grown) return;
    memset(grown, 0, capacity * 2u * sizeof(*grown));
    {
        RinPthreadCancelRecord** old = g_rin_pthread_cancel_buckets;
        int old_dynamic = g_rin_pthread_cancel_buckets_dynamic;
        /* Install the new array before rehashing so inserts land in it.
         * A fork torn between the two publishes is harmless: the child
         * rebuilds the index from the all-records list. */
        g_rin_pthread_cancel_buckets = grown;
        g_rin_pthread_cancel_bucket_mask = capacity * 2u - 1u;
        for (index = 0u; index < capacity; ++index) {
            RinPthreadCancelRecord* record = old[index];
            while (record) {
                RinPthreadCancelRecord* next = record->hash_next;
                rin_pthread_cancel_bucket_insert_locked(record);
                record = next;
            }
        }
        if (old_dynamic) _RIN_PTHREAD_CANCEL_FREE(old);
    }
    g_rin_pthread_cancel_buckets_dynamic = 1;
}

static void rin_pthread_cancel_insert_locked(
    RinPthreadCancelRecord* record) {
    rin_pthread_cancel_bucket_insert_locked(record);
    record->all_next = g_rin_pthread_cancel_all;
    g_rin_pthread_cancel_all = record;
    ++g_rin_pthread_cancel_count;
    rin_pthread_cancel_maybe_grow_locked();
}

static void rin_pthread_cancel_remove_locked(
    RinPthreadCancelRecord* record) {
    size_t slot = rin_pthread_cancel_hash(record->thread) &
        g_rin_pthread_cancel_bucket_mask;
    RinPthreadCancelRecord** link =
        &g_rin_pthread_cancel_buckets[slot];
    RinPthreadCancelRecord** all_link = &g_rin_pthread_cancel_all;
    while (*link && *link != record) link = &(*link)->hash_next;
    if (*link == record) *link = record->hash_next;
    while (*all_link && *all_link != record)
        all_link = &(*all_link)->all_next;
    if (*all_link == record) *all_link = record->all_next;
    record->hash_next = NULL;
    record->all_next = NULL;
    if (g_rin_pthread_cancel_count != 0u) --g_rin_pthread_cancel_count;
}

static void rin_pthread_cancel_initialize_record(
    RinPthreadCancelRecord* record, pthread_t thread) {
    record->thread = thread;
    __atomic_store_n(&record->pending, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&record->state, PTHREAD_CANCEL_ENABLE,
                     __ATOMIC_RELAXED);
    __atomic_store_n(&record->type, PTHREAD_CANCEL_DEFERRED,
                     __ATOMIC_RELAXED);
    __atomic_store_n(&record->active, 1, __ATOMIC_RELAXED);
    record->detached = 0;
    record->async_defer = 0;
    record->exit_value = NULL;
    record->cleanup_top = NULL;
    record->hash_next = NULL;
    record->all_next = NULL;
}

/* Fork prepare/parent/child run through the atfork registry so the cancel
 * index stays consistent across fork without a dedicated libc init hook.
 * Registration is lazy (first slow path) and retried while it fails. */
static void rin_pthread_cancel_fork_prepare(void);
static void rin_pthread_cancel_fork_parent(void);
static void rin_pthread_cancel_fork_child(void);

static void rin_pthread_cancel_ensure_fork_hooks_locked(void) {
    if (!g_rin_pthread_cancel_fork_hooked) {
        if (pthread_atfork(rin_pthread_cancel_fork_prepare,
                           rin_pthread_cancel_fork_parent,
                           rin_pthread_cancel_fork_child) == 0)
            g_rin_pthread_cancel_fork_hooked = 1;
    }
}

static void rin_pthread_cancel_fork_prepare(void) {
    rin_pthread_cancel_lock(_RIN_PTHREAD_CANCEL_SELF());
}

static void rin_pthread_cancel_fork_parent(void) {
    rin_pthread_cancel_unlock();
}

static void rin_pthread_cancel_fork_child(void) {
    pthread_t self = _RIN_PTHREAD_CANCEL_SELF();
    RinPthreadCancelRecord* record;
    RinPthreadCancelRecord* next;
    RinPthreadCancelRecord* kept = NULL;
    size_t index;
    size_t capacity;
    /* The child is single-threaded: reset the lock even if a dead thread
     * held it, drop the grown bucket array, and rebuild the index with
     * only the surviving thread.  A record inserted by a torn fork-time
     * registration belongs to a dead thread and is simply freed. */
    g_rin_pthread_cancel_lock = 0u;
    g_rin_pthread_cancel_lock_owner = (pthread_t)0;
    g_rin_pthread_cancel_lock_depth = 0u;
    if (g_rin_pthread_cancel_buckets_dynamic) {
        _RIN_PTHREAD_CANCEL_FREE(g_rin_pthread_cancel_buckets);
        g_rin_pthread_cancel_buckets_dynamic = 0;
    }
    g_rin_pthread_cancel_buckets =
        g_rin_pthread_cancel_static_buckets;
    g_rin_pthread_cancel_bucket_mask =
        RIN_PTHREAD_CANCEL_HASH_INITIAL - 1u;
    capacity = RIN_PTHREAD_CANCEL_HASH_INITIAL;
    for (index = 0u; index < capacity; ++index)
        g_rin_pthread_cancel_static_buckets[index] = NULL;
    for (record = g_rin_pthread_cancel_all; record != NULL;
         record = next) {
        next = record->all_next;
        if (record->thread == self && kept == NULL) {
            kept = record;
            kept->hash_next = NULL;
            kept->all_next = NULL;
        } else {
            _RIN_PTHREAD_CANCEL_FREE(record);
        }
    }
    g_rin_pthread_cancel_all = NULL;
    g_rin_pthread_cancel_count = 0u;
    if (kept != NULL) {
        rin_pthread_cancel_insert_locked(kept);
        t_rin_pthread_cancel_self = kept;
    } else {
        t_rin_pthread_cancel_self = NULL;
    }
}

static RinPthreadCancelRecord* rin_pthread_cancel_self_cached(
    pthread_t self) {
    RinPthreadCancelRecord* cached = t_rin_pthread_cancel_self;
    if (cached != NULL && cached->thread == self) return cached;
    return NULL;
}

/* Adopt an existing record for the calling thread without creating one.
 * Used by pure queries; returns NULL when no record exists. */
static RinPthreadCancelRecord* rin_pthread_cancel_adopt_self(
    pthread_t self) {
    RinPthreadCancelRecord* record;
    RinPthreadCancelRecord* cached = rin_pthread_cancel_self_cached(self);
    if (cached != NULL) return cached;
    if (self == 0u) return NULL;
    rin_pthread_cancel_lock(self);
    record = rin_pthread_cancel_find_locked(self);
    if (record != NULL) t_rin_pthread_cancel_self = record;
    rin_pthread_cancel_unlock();
    return record;
}

/* Register the calling thread, creating its record on first use.  At most
 * one slow path per thread lifetime; every later cancellation point is
 * lock-free. */
static RinPthreadCancelRecord* rin_pthread_cancel_register_self(
    pthread_t self) {
    RinPthreadCancelRecord* record;
    RinPthreadCancelRecord* cached = rin_pthread_cancel_self_cached(self);
    if (cached != NULL) return cached;
    if (self == 0u) return NULL;
    rin_pthread_cancel_lock(self);
    rin_pthread_cancel_ensure_fork_hooks_locked();
    record = rin_pthread_cancel_find_locked(self);
    if (record == NULL) {
        record = (RinPthreadCancelRecord*)_RIN_PTHREAD_CANCEL_ALLOC(
            sizeof(*record));
        if (record != NULL) {
            rin_pthread_cancel_initialize_record(record, self);
            rin_pthread_cancel_insert_locked(record);
        }
    } else if (!__atomic_load_n(&record->active, __ATOMIC_ACQUIRE)) {
        rin_pthread_cancel_initialize_record(record, self);
    }
    if (record != NULL) t_rin_pthread_cancel_self = record;
    rin_pthread_cancel_unlock();
    return record;
}

int rin_pthread_cancel_register(pthread_t thread) {
    pthread_t self = _RIN_PTHREAD_CANCEL_SELF();
    RinPthreadCancelRecord* record;
    if (thread == 0u) return EINVAL;
    if (thread == self) {
        record = rin_pthread_cancel_self_cached(self);
        if (record != NULL &&
            __atomic_load_n(&record->active, __ATOMIC_ACQUIRE))
            return 0;
        return rin_pthread_cancel_register_self(self) != NULL ? 0
                                                              : EAGAIN;
    }
    rin_pthread_cancel_lock(self);
    rin_pthread_cancel_ensure_fork_hooks_locked();
    record = rin_pthread_cancel_find_locked(thread);
    if (record == NULL) {
        record = (RinPthreadCancelRecord*)_RIN_PTHREAD_CANCEL_ALLOC(
            sizeof(*record));
        if (record == NULL) {
            rin_pthread_cancel_unlock();
            return EAGAIN;
        }
        rin_pthread_cancel_initialize_record(record, thread);
        rin_pthread_cancel_insert_locked(record);
    } else if (!__atomic_load_n(&record->active, __ATOMIC_ACQUIRE)) {
        rin_pthread_cancel_initialize_record(record, thread);
    }
    rin_pthread_cancel_unlock();
    return 0;
}

int rin_pthread_cancel_register_created(pthread_t thread) {
    pthread_t self = _RIN_PTHREAD_CANCEL_SELF();
    RinPthreadCancelRecord* record;
    if (thread == 0u) return EINVAL;
    rin_pthread_cancel_lock(self);
    rin_pthread_cancel_ensure_fork_hooks_locked();
    record = rin_pthread_cancel_find_locked(thread);
    if (record == NULL) {
        record = (RinPthreadCancelRecord*)_RIN_PTHREAD_CANCEL_ALLOC(
            sizeof(*record));
        if (record == NULL) {
            rin_pthread_cancel_unlock();
            return EAGAIN;
        }
        rin_pthread_cancel_initialize_record(record, thread);
        rin_pthread_cancel_insert_locked(record);
    }
    /* The child may have registered and exited before clone returned here.
     * Preserve that exit record instead of reviving a dead TID.  Kernel
     * TIDs are monotonic per process, so a stale record can never match a
     * new thread. */
    rin_pthread_cancel_unlock();
    return 0;
}

void rin_pthread_cancel_set_detached(pthread_t thread) {
    pthread_t self = _RIN_PTHREAD_CANCEL_SELF();
    RinPthreadCancelRecord* record;
    if (thread == 0u) return;
    rin_pthread_cancel_lock(self);
    record = rin_pthread_cancel_find_locked(thread);
    if (record != NULL) {
        record->detached = 1;
        if (!__atomic_load_n(&record->active, __ATOMIC_ACQUIRE)) {
            rin_pthread_cancel_remove_locked(record);
            if (t_rin_pthread_cancel_self == record)
                t_rin_pthread_cancel_self = NULL;
            _RIN_PTHREAD_CANCEL_FREE(record);
        }
    }
    rin_pthread_cancel_unlock();
}

void rin_pthread_cancel_exit(pthread_t thread, void* value) {
    pthread_t self = _RIN_PTHREAD_CANCEL_SELF();
    RinPthreadCancelRecord* record;
    if (thread == 0u) return;
    if (thread == self) t_rin_pthread_cancel_self = NULL;
    rin_pthread_cancel_lock(self);
    record = rin_pthread_cancel_find_locked(thread);
    if (record != NULL) {
        __atomic_store_n(&record->active, 0, __ATOMIC_RELEASE);
        __atomic_store_n(&record->pending, 0, __ATOMIC_RELAXED);
        record->exit_value = value;
        record->cleanup_top = NULL;
        if (record->detached) {
            rin_pthread_cancel_remove_locked(record);
            if (t_rin_pthread_cancel_self == record)
                t_rin_pthread_cancel_self = NULL;
            _RIN_PTHREAD_CANCEL_FREE(record);
        }
    }
    rin_pthread_cancel_unlock();
}

void rin_pthread_cancel_take_exit(pthread_t thread, void** value) {
    pthread_t self = _RIN_PTHREAD_CANCEL_SELF();
    RinPthreadCancelRecord* record;
    if (value) *value = NULL;
    if (thread == 0u) return;
    rin_pthread_cancel_lock(self);
    record = rin_pthread_cancel_find_locked(thread);
    if (record != NULL) {
        if (value) *value = record->exit_value;
        rin_pthread_cancel_remove_locked(record);
        if (t_rin_pthread_cancel_self == record)
            t_rin_pthread_cancel_self = NULL;
        _RIN_PTHREAD_CANCEL_FREE(record);
    }
    rin_pthread_cancel_unlock();
}

/* Consume a pending request.  Lock-free: the pending word is only ever
 * set by cancellers and cleared here on the owning thread. */
static int rin_pthread_cancel_consume_enabled(
    RinPthreadCancelRecord* record) {
    if (!__atomic_load_n(&record->active, __ATOMIC_ACQUIRE))
        return 0;
    if (__atomic_load_n(&record->state, __ATOMIC_ACQUIRE) !=
        PTHREAD_CANCEL_ENABLE)
        return 0;
    return __atomic_exchange_n(&record->pending, 0,
                               __ATOMIC_ACQ_REL) != 0;
}

static int rin_pthread_cancel_consume_any(
    RinPthreadCancelRecord* record) {
    if (!__atomic_load_n(&record->active, __ATOMIC_ACQUIRE))
        return 0;
    return __atomic_exchange_n(&record->pending, 0,
                               __ATOMIC_ACQ_REL) != 0;
}

static int rin_pthread_setcancelstate_impl(int state, int* oldstate) {
    pthread_t self = _RIN_PTHREAD_CANCEL_SELF();
    RinPthreadCancelRecord* record;
    int previous;
    if (state != PTHREAD_CANCEL_ENABLE && state != PTHREAD_CANCEL_DISABLE)
        return EINVAL;
    record = rin_pthread_cancel_register_self(self);
    if (record == NULL) return EAGAIN;
    previous = __atomic_exchange_n(&record->state, state,
                                   __ATOMIC_ACQ_REL);
    if (oldstate) *oldstate = previous;
    if (state == PTHREAD_CANCEL_ENABLE) pthread_testcancel();
    return 0;
}

int pthread_setcancelstate(int state, int* oldstate) {
    int saved_errno = errno;
    int result = rin_pthread_setcancelstate_impl(state, oldstate);
    errno = saved_errno;
    return result;
}

static int rin_pthread_setcanceltype_impl(int type, int* oldtype) {
    pthread_t self = _RIN_PTHREAD_CANCEL_SELF();
    RinPthreadCancelRecord* record;
    int previous;
    if (type != PTHREAD_CANCEL_DEFERRED &&
        type != PTHREAD_CANCEL_ASYNCHRONOUS)
        return EINVAL;
    if (type == PTHREAD_CANCEL_ASYNCHRONOUS) return ENOTSUP;
    record = rin_pthread_cancel_register_self(self);
    if (record == NULL) return EAGAIN;
    previous = __atomic_exchange_n(&record->type, type,
                                   __ATOMIC_ACQ_REL);
    if (oldtype) *oldtype = previous;
    return 0;
}

int pthread_setcanceltype(int type, int* oldtype) {
    int saved_errno = errno;
    int result = rin_pthread_setcanceltype_impl(type, oldtype);
    errno = saved_errno;
    return result;
}

int pthread_cancel(pthread_t thread) {
    pthread_t self = _RIN_PTHREAD_CANCEL_SELF();
    RinPthreadCancelRecord* record;
    int should_wake = 0;
    int saved_errno = errno;
    if (thread == 0u) {
        errno = saved_errno;
        return ESRCH;
    }
    rin_pthread_cancel_lock(self);
    record = rin_pthread_cancel_find_locked(thread);
    if (record != NULL &&
        __atomic_load_n(&record->active, __ATOMIC_ACQUIRE)) {
        __atomic_store_n(&record->pending, 1, __ATOMIC_RELEASE);
        should_wake = __atomic_load_n(&record->state,
                                      __ATOMIC_ACQUIRE) ==
            PTHREAD_CANCEL_ENABLE;
        rin_pthread_cancel_unlock();
    } else {
        rin_pthread_cancel_unlock();
        /* The initial process thread does not pass through
         * pthread_create().  Admit a live, otherwise-unregistered thread
         * before recording a request so it can be cancelled by another
         * pthread. */
        {
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
        rin_pthread_cancel_lock(self);
        record = rin_pthread_cancel_find_locked(thread);
        if (record == NULL ||
            !__atomic_load_n(&record->active, __ATOMIC_ACQUIRE)) {
            rin_pthread_cancel_unlock();
            errno = saved_errno;
            return ESRCH;
        }
        __atomic_store_n(&record->pending, 1, __ATOMIC_RELEASE);
        should_wake = __atomic_load_n(&record->state,
                                      __ATOMIC_ACQUIRE) ==
            PTHREAD_CANCEL_ENABLE;
        rin_pthread_cancel_unlock();
    }
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
            rin_pthread_cancel_lock(self);
            record = rin_pthread_cancel_find_locked(thread);
            if (record != NULL &&
                !__atomic_load_n(&record->active, __ATOMIC_ACQUIRE))
                wake_error = 0;
            rin_pthread_cancel_unlock();
            errno = saved_errno;
            return wake_error;
        }
    }
    errno = saved_errno;
    return 0;
}

int rin_pthread_cancel_pending_enabled(pthread_t thread) {
    pthread_t self = _RIN_PTHREAD_CANCEL_SELF();
    RinPthreadCancelRecord* record;
    if (thread == 0u) return 0;
    if (thread == self) {
        record = rin_pthread_cancel_self_cached(self);
        if (record == NULL)
            record = rin_pthread_cancel_adopt_self(self);
    } else {
        rin_pthread_cancel_lock(self);
        record = rin_pthread_cancel_find_locked(thread);
        if (record != NULL &&
            (!__atomic_load_n(&record->active, __ATOMIC_ACQUIRE) ||
             __atomic_load_n(&record->state, __ATOMIC_ACQUIRE) !=
                 PTHREAD_CANCEL_ENABLE ||
             !__atomic_load_n(&record->pending, __ATOMIC_ACQUIRE)))
            record = NULL;
        rin_pthread_cancel_unlock();
        return record != NULL;
    }
    if (record == NULL) return 0;
    return __atomic_load_n(&record->active, __ATOMIC_ACQUIRE) &&
        __atomic_load_n(&record->state, __ATOMIC_ACQUIRE) ==
            PTHREAD_CANCEL_ENABLE &&
        __atomic_load_n(&record->pending, __ATOMIC_ACQUIRE);
}

void __rin_pthread_cancel_point(void) {
    pthread_t self = _RIN_PTHREAD_CANCEL_SELF();
    RinPthreadCancelRecord* record =
        rin_pthread_cancel_self_cached(self);
    int saved_errno = errno;
    if (record == NULL)
        record = rin_pthread_cancel_register_self(self);
    if (record != NULL &&
        __atomic_load_n(&record->active, __ATOMIC_ACQUIRE) &&
        __atomic_load_n(&record->state, __ATOMIC_ACQUIRE) ==
            PTHREAD_CANCEL_ENABLE &&
        __atomic_load_n(&record->pending, __ATOMIC_ACQUIRE)) {
        errno = saved_errno;
        pthread_testcancel();
        return;
    }
    errno = saved_errno;
}

void __rin_pthread_cleanup_push_node(__rin_pthread_cleanup_buffer* buffer,
                                     void (*routine)(void*), void* argument) {
    pthread_t self = _RIN_PTHREAD_CANCEL_SELF();
    RinPthreadCancelRecord* record;
    if (buffer == NULL || routine == NULL) return;
    record = rin_pthread_cancel_self_cached(self);
    if (record == NULL)
        record = rin_pthread_cancel_register_self(self);
    if (record == NULL) return;
    buffer->routine = routine;
    buffer->argument = argument;
    buffer->active = 1;
    buffer->next = record->cleanup_top;
    record->cleanup_top = buffer;
}

void __rin_pthread_cleanup_pop_node(int execute) {
    pthread_t self = _RIN_PTHREAD_CANCEL_SELF();
    RinPthreadCancelRecord* record =
        rin_pthread_cancel_self_cached(self);
    __rin_pthread_cleanup_buffer* buffer = NULL;
    if (record != NULL && record->cleanup_top != NULL) {
        buffer = record->cleanup_top;
        record->cleanup_top = buffer->next;
        buffer->next = NULL;
        buffer->active = 0;
    }
    if (execute && buffer != NULL && buffer->routine != NULL)
        buffer->routine(buffer->argument);
}

void rin_pthread_cancel_run_cleanups(pthread_t thread) {
    pthread_t self = _RIN_PTHREAD_CANCEL_SELF();
    if (thread == self) {
        RinPthreadCancelRecord* record =
            rin_pthread_cancel_self_cached(self);
        while (record != NULL && record->cleanup_top != NULL) {
            __rin_pthread_cleanup_buffer* buffer =
                record->cleanup_top;
            record->cleanup_top = buffer->next;
            buffer->next = NULL;
            buffer->active = 0;
            if (buffer->routine != NULL)
                buffer->routine(buffer->argument);
        }
        return;
    }
    for (;;) {
        RinPthreadCancelRecord* record;
        __rin_pthread_cleanup_buffer* buffer;
        rin_pthread_cancel_lock(self);
        record = rin_pthread_cancel_find_locked(thread);
        buffer = record != NULL ? record->cleanup_top : NULL;
        if (buffer != NULL) {
            record->cleanup_top = buffer->next;
            buffer->next = NULL;
            buffer->active = 0;
        }
        rin_pthread_cancel_unlock();
        if (buffer == NULL) break;
        if (buffer->routine != NULL)
            buffer->routine(buffer->argument);
    }
}

void pthread_testcancel(void) {
    pthread_t self = _RIN_PTHREAD_CANCEL_SELF();
    RinPthreadCancelRecord* record =
        rin_pthread_cancel_self_cached(self);
    int saved_errno = errno;
    if (record == NULL)
        record = rin_pthread_cancel_register_self(self);
    if (record != NULL && rin_pthread_cancel_consume_enabled(record)) {
        errno = saved_errno;
        pthread_exit(PTHREAD_CANCELED);
    }
    errno = saved_errno;
}

void __rin_pthread_cancel_point_cleanup(
    void (*cleanup)(void*), void* argument) {
    pthread_t self = _RIN_PTHREAD_CANCEL_SELF();
    RinPthreadCancelRecord* record;
    int old_state = PTHREAD_CANCEL_ENABLE;
    if (pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &old_state) != 0) {
        pthread_testcancel();
        return;
    }
    if (old_state != PTHREAD_CANCEL_ENABLE) {
        (void)pthread_setcancelstate(old_state, NULL);
        return;
    }

    record = rin_pthread_cancel_self_cached(self);
    if (record != NULL && rin_pthread_cancel_consume_any(record)) {
        if (cleanup) cleanup(argument);
        (void)pthread_setcancelstate(old_state, NULL);
        pthread_exit(PTHREAD_CANCELED);
    }

    (void)pthread_setcancelstate(old_state, NULL);
    pthread_testcancel();
}
