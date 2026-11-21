/* SPDX-License-Identifier: MIT */
/* Product-owned POSIX pthread_atfork registry and fork dispatch hooks. */

#include "pthread.h"
#include "internal/pthread_atfork.h"
#include "stdlib.h"

static RinPthreadAtforkChunk g_rin_pthread_atfork_initial_chunk;
static RinPthreadAtforkChunk* g_rin_pthread_atfork_head =
    &g_rin_pthread_atfork_initial_chunk;
static RinPthreadAtforkChunk* g_rin_pthread_atfork_tail =
    &g_rin_pthread_atfork_initial_chunk;
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
    RinPthreadAtforkChunk* candidate = NULL;
    size_t index;
    int installed;

    for (;;) {
        rin_pthread_atfork_lock();
        index = g_rin_pthread_atfork_count;
        if (index < RIN_PTHREAD_ATFORK_CHUNK_ENTRIES ||
            index % RIN_PTHREAD_ATFORK_CHUNK_ENTRIES != 0u) {
            index %= RIN_PTHREAD_ATFORK_CHUNK_ENTRIES;
            g_rin_pthread_atfork_tail->entries[index].prepare = prepare;
            g_rin_pthread_atfork_tail->entries[index].parent = parent;
            g_rin_pthread_atfork_tail->entries[index].child = child;
            __atomic_store_n(&g_rin_pthread_atfork_count,
                             g_rin_pthread_atfork_count + 1u,
                             __ATOMIC_RELEASE);
            rin_pthread_atfork_unlock();
            if (candidate != NULL) free(candidate);
            return 0;
        }
        rin_pthread_atfork_unlock();

        if (candidate == NULL) {
            candidate = (RinPthreadAtforkChunk*)malloc(sizeof(*candidate));
            if (candidate == NULL) return ENOMEM;
            candidate->next = NULL;
            candidate->prev = NULL;
        }

        rin_pthread_atfork_lock();
        installed = 0;
        index = g_rin_pthread_atfork_count;
        if (index >= RIN_PTHREAD_ATFORK_CHUNK_ENTRIES &&
            index % RIN_PTHREAD_ATFORK_CHUNK_ENTRIES == 0u) {
            candidate->prev = g_rin_pthread_atfork_tail;
            g_rin_pthread_atfork_tail->next = candidate;
            g_rin_pthread_atfork_tail = candidate;
            candidate = NULL;
            installed = 1;
        }
        if (index < RIN_PTHREAD_ATFORK_CHUNK_ENTRIES ||
            index % RIN_PTHREAD_ATFORK_CHUNK_ENTRIES != 0u || installed) {
            index %= RIN_PTHREAD_ATFORK_CHUNK_ENTRIES;
            g_rin_pthread_atfork_tail->entries[index].prepare = prepare;
            g_rin_pthread_atfork_tail->entries[index].parent = parent;
            g_rin_pthread_atfork_tail->entries[index].child = child;
            __atomic_store_n(&g_rin_pthread_atfork_count,
                             g_rin_pthread_atfork_count + 1u,
                             __ATOMIC_RELEASE);
            rin_pthread_atfork_unlock();
            if (candidate != NULL) free(candidate);
            return 0;
        }
        rin_pthread_atfork_unlock();
    }
}

void __rin_pthread_atfork_child_reset(void) {
    /* A second thread may have held the append lock at the instant fork
     * cloned the address space.  Its owner does not exist in the child. */
    __atomic_store_n(&g_rin_pthread_atfork_lock, 0u, __ATOMIC_RELEASE);
}

void __rin_pthread_atfork_prepare(
    __rin_pthread_atfork_snapshot* snapshot) {
    RinPthreadAtforkChunk* chunk;
    size_t remaining;
    if (!snapshot) return;
    rin_pthread_atfork_lock();
    snapshot->count = __atomic_load_n(&g_rin_pthread_atfork_count,
                                      __ATOMIC_ACQUIRE);
    snapshot->tail = g_rin_pthread_atfork_tail;
    rin_pthread_atfork_unlock();

    remaining = snapshot->count;
    chunk = snapshot->tail;
    while (chunk != NULL && remaining != 0u) {
        size_t count = remaining % RIN_PTHREAD_ATFORK_CHUNK_ENTRIES;
        if (count == 0u) count = RIN_PTHREAD_ATFORK_CHUNK_ENTRIES;
        while (count != 0u) {
            void (*handler)(void) = chunk->entries[--count].prepare;
            if (handler) handler();
        }
        remaining -= remaining < RIN_PTHREAD_ATFORK_CHUNK_ENTRIES
            ? remaining : RIN_PTHREAD_ATFORK_CHUNK_ENTRIES;
        chunk = chunk->prev;
    }
}

void __rin_pthread_atfork_parent(
    const __rin_pthread_atfork_snapshot* snapshot) {
    RinPthreadAtforkChunk* chunk;
    size_t remaining;
    if (!snapshot) return;
    remaining = snapshot->count;
    chunk = g_rin_pthread_atfork_head;
    while (chunk != NULL && remaining != 0u) {
        size_t count = remaining < RIN_PTHREAD_ATFORK_CHUNK_ENTRIES
            ? remaining : RIN_PTHREAD_ATFORK_CHUNK_ENTRIES;
        for (size_t index = 0u; index < count; ++index) {
            void (*handler)(void) = chunk->entries[index].parent;
            if (handler) handler();
        }
        remaining -= count;
        if (chunk == snapshot->tail) break;
        chunk = chunk->next;
    }
}

void __rin_pthread_atfork_child(
    const __rin_pthread_atfork_snapshot* snapshot) {
    RinPthreadAtforkChunk* chunk;
    size_t remaining;
    if (!snapshot) return;
    remaining = snapshot->count;
    chunk = g_rin_pthread_atfork_head;
    while (chunk != NULL && remaining != 0u) {
        size_t count = remaining < RIN_PTHREAD_ATFORK_CHUNK_ENTRIES
            ? remaining : RIN_PTHREAD_ATFORK_CHUNK_ENTRIES;
        for (size_t index = 0u; index < count; ++index) {
            void (*handler)(void) = chunk->entries[index].child;
            if (handler) handler();
        }
        remaining -= count;
        if (chunk == snapshot->tail) break;
        chunk = chunk->next;
    }
}
