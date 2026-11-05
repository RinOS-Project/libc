/* SPDX-License-Identifier: MIT */
/* Product-owned POSIX pthread_atfork registry and fork dispatch hooks. */

#include "pthread.h"
#include "internal/pthread_atfork.h"

typedef struct RinPthreadAtforkEntry {
    void (*prepare)(void);
    void (*parent)(void);
    void (*child)(void);
} RinPthreadAtforkEntry;

static RinPthreadAtforkEntry g_rin_pthread_atfork_entries[
    RIN_PTHREAD_ATFORK_MAX];
static size_t g_rin_pthread_atfork_count;
static volatile unsigned int g_rin_pthread_atfork_lock;

static void rin_pthread_atfork_lock(void) {
    while (__atomic_exchange_n(&g_rin_pthread_atfork_lock, 1u,
                               __ATOMIC_ACQUIRE) != 0u) {
#if defined(__i386__) || defined(__x86_64__)
        __asm__ volatile("pause" ::: "memory");
#else
        __asm__ volatile("" ::: "memory");
#endif
    }
}

static void rin_pthread_atfork_unlock(void) {
    __atomic_store_n(&g_rin_pthread_atfork_lock, 0u, __ATOMIC_RELEASE);
}

int pthread_atfork(void (*prepare)(void), void (*parent)(void),
                   void (*child)(void)) {
    size_t index;
    rin_pthread_atfork_lock();
    index = g_rin_pthread_atfork_count;
    if (index >= RIN_PTHREAD_ATFORK_MAX) {
        rin_pthread_atfork_unlock();
        return ENOMEM;
    }
    g_rin_pthread_atfork_entries[index].prepare = prepare;
    g_rin_pthread_atfork_entries[index].parent = parent;
    g_rin_pthread_atfork_entries[index].child = child;
    __atomic_store_n(&g_rin_pthread_atfork_count, index + 1u,
                     __ATOMIC_RELEASE);
    rin_pthread_atfork_unlock();
    return 0;
}

void __rin_pthread_atfork_child_reset(void) {
    /* A second thread may have held the append lock at the instant fork
     * cloned the address space.  Its owner does not exist in the child. */
    __atomic_store_n(&g_rin_pthread_atfork_lock, 0u, __ATOMIC_RELEASE);
}

void __rin_pthread_atfork_prepare(
    __rin_pthread_atfork_snapshot* snapshot) {
    size_t count;
    if (!snapshot) return;
    rin_pthread_atfork_lock();
    count = __atomic_load_n(&g_rin_pthread_atfork_count, __ATOMIC_ACQUIRE);
    rin_pthread_atfork_unlock();
    if (count > RIN_PTHREAD_ATFORK_MAX) count = RIN_PTHREAD_ATFORK_MAX;
    snapshot->count = count;
    while (count != 0u) {
        void (*handler)(void) = g_rin_pthread_atfork_entries[--count].prepare;
        if (handler) handler();
    }
}

void __rin_pthread_atfork_parent(
    const __rin_pthread_atfork_snapshot* snapshot) {
    size_t index;
    size_t count;
    if (!snapshot) return;
    count = snapshot->count;
    if (count > RIN_PTHREAD_ATFORK_MAX) count = RIN_PTHREAD_ATFORK_MAX;
    for (index = 0u; index < count; ++index) {
        void (*handler)(void) = g_rin_pthread_atfork_entries[index].parent;
        if (handler) handler();
    }
}

void __rin_pthread_atfork_child(
    const __rin_pthread_atfork_snapshot* snapshot) {
    size_t index;
    size_t count;
    if (!snapshot) return;
    count = snapshot->count;
    if (count > RIN_PTHREAD_ATFORK_MAX) count = RIN_PTHREAD_ATFORK_MAX;
    for (index = 0u; index < count; ++index) {
        void (*handler)(void) = g_rin_pthread_atfork_entries[index].child;
        if (handler) handler();
    }
}
