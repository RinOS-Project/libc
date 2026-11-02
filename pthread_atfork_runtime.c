/* SPDX-License-Identifier: MIT */
/* Product-owned POSIX pthread_atfork registry and fork dispatch hooks. */

#include "pthread.h"
#include "stdlib.h"
#include "string.h"
#include "errno.h"
#include "internal/pthread_atfork.h"

#ifndef _RIN_PTHREAD_ATFORK_ALLOC
#define _RIN_PTHREAD_ATFORK_ALLOC(size) malloc(size)
#endif
#ifndef _RIN_PTHREAD_ATFORK_FREE
#define _RIN_PTHREAD_ATFORK_FREE(pointer) free(pointer)
#endif
/* Test hooks: run with the registry lock held (LOCKED, after acquire) or
 * about to be released (UNLOCKING, before release) so tests can track
 * per-thread ownership.  Default to nothing in production. */
#ifndef _RIN_PTHREAD_ATFORK_LOCKED
#define _RIN_PTHREAD_ATFORK_LOCKED() ((void)0)
#endif
#ifndef _RIN_PTHREAD_ATFORK_UNLOCKING
#define _RIN_PTHREAD_ATFORK_UNLOCKING() ((void)0)
#endif

static RinPthreadAtforkChunk g_rin_pthread_atfork_head;
static RinPthreadAtforkChunk* g_rin_pthread_atfork_tail =
    &g_rin_pthread_atfork_head;
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
    _RIN_PTHREAD_ATFORK_LOCKED();
}

static void rin_pthread_atfork_unlock(void) {
    _RIN_PTHREAD_ATFORK_UNLOCKING();
    __atomic_store_n(&g_rin_pthread_atfork_lock, 0u, __ATOMIC_RELEASE);
}

int pthread_atfork(void (*prepare)(void), void (*parent)(void),
                   void (*child)(void)) {
    int saved_errno = errno;
    size_t slot;
    RinPthreadAtforkChunk* tail;
    RinPthreadAtforkEntry entry;

    entry.prepare = prepare;
    entry.parent = parent;
    entry.child = child;
    rin_pthread_atfork_lock();
    slot = g_rin_pthread_atfork_count % RIN_PTHREAD_ATFORK_CHUNK_ENTRIES;
    tail = g_rin_pthread_atfork_tail;
    if (slot != 0u || g_rin_pthread_atfork_count == 0u) {
        tail->entries[slot] = entry;
        __atomic_store_n(&g_rin_pthread_atfork_count,
                         g_rin_pthread_atfork_count + 1u, __ATOMIC_RELEASE);
        rin_pthread_atfork_unlock();
        errno = saved_errno;
        return 0;
    }
    rin_pthread_atfork_unlock();

    /* Slow path: the chunk boundary needs an overflow chunk.  Allocate
     * and initialize the candidate outside the registry lock so a slow
     * allocator or page fault never stalls concurrent registrants or a
     * fork snapshot spinning on the lock.  The candidate is thread
     * private until published, so a fork racing the allocation cannot
     * observe it. */
    {
        RinPthreadAtforkChunk* chunk = (RinPthreadAtforkChunk*)
            _RIN_PTHREAD_ATFORK_ALLOC(sizeof(RinPthreadAtforkChunk));
        int consumed = 0;
        if (!chunk) {
            errno = saved_errno;
            return ENOMEM;
        }
        chunk->next = NULL;
        memset(chunk->entries, 0, sizeof(chunk->entries));
        chunk->entries[0] = entry;
        /* Short critical section: re-verify the count and tail, publish
         * at most once.  A racing thread may have crossed the boundary
         * first; then this entry lands in a plain slot and the unused
         * candidate is freed below, outside the lock. */
        rin_pthread_atfork_lock();
        slot = g_rin_pthread_atfork_count %
            RIN_PTHREAD_ATFORK_CHUNK_ENTRIES;
        tail = g_rin_pthread_atfork_tail;
        if (slot == 0u && g_rin_pthread_atfork_count != 0u) {
            chunk->prev = tail;
            /* Publish order matters for a fork that lands
             * mid-registration: entry first, then the link, then the
             * tail, then the count.  A torn child misses the incomplete
             * entry (the count is rebuilt from committed state in
             * __rin_pthread_atfork_child_reset and unwritten slots stay
             * NULL); it never runs a torn entry. */
            tail->next = chunk;
            __atomic_store_n(&g_rin_pthread_atfork_tail, chunk,
                             __ATOMIC_RELEASE);
            consumed = 1;
        } else {
            tail->entries[slot] = entry;
        }
        __atomic_store_n(&g_rin_pthread_atfork_count,
                         g_rin_pthread_atfork_count + 1u, __ATOMIC_RELEASE);
        rin_pthread_atfork_unlock();
        if (!consumed)
            _RIN_PTHREAD_ATFORK_FREE(chunk);
    }
    errno = saved_errno;
    return 0;
}

void __rin_pthread_atfork_child_reset(void) {
    RinPthreadAtforkChunk* chunk;
    size_t count;
    size_t chunks;
    size_t step;
    /* A second thread may have held the append lock at the instant fork
     * cloned the address space.  Its owner does not exist in the child. */
    __atomic_store_n(&g_rin_pthread_atfork_lock, 0u, __ATOMIC_RELEASE);
    /* Rebuild the tail from the committed count.  The count is stored
     * last.  At a chunk boundary, a linked successor is already fully
     * initialized before its link is published, even if fork copied the
     * address space before the count increment.  Adopt that one complete
     * entry instead of cutting off the chunk and losing the registration.
     * The child is single-threaded, so no lock is needed. */
    count = g_rin_pthread_atfork_count;
    chunk = &g_rin_pthread_atfork_head;
    if (count == 0u) {
        g_rin_pthread_atfork_tail = chunk;
        chunk->next = NULL;
        return;
    }
    chunks = (count - 1u) /
        RIN_PTHREAD_ATFORK_CHUNK_ENTRIES + 1u;
    for (step = 1u; step < chunks && chunk->next != NULL; ++step)
        chunk = chunk->next;
    if ((count % RIN_PTHREAD_ATFORK_CHUNK_ENTRIES) == 0u &&
        chunk->next != NULL) {
        chunk = chunk->next;
        ++count;
        __atomic_store_n(&g_rin_pthread_atfork_count, count,
                         __ATOMIC_RELEASE);
    }
    g_rin_pthread_atfork_tail = chunk;
    chunk->next = NULL;
}

/* Count the chunks from the head to the snapshot tail and derive how many
 * snapshot entries the tail chunk holds.  Every chunk ahead of the tail is
 * full: only the newest link could have torn, and a torn link leaves the
 * tail empty (fill 0), never over-full. */
static size_t rin_pthread_atfork_tail_fill(
    const __rin_pthread_atfork_snapshot* snapshot) {
    RinPthreadAtforkChunk* chunk = &g_rin_pthread_atfork_head;
    size_t full_before_tail = 0u;
    size_t bound;
    if (!snapshot || !snapshot->tail) return 0u;
    bound = snapshot->count / RIN_PTHREAD_ATFORK_CHUNK_ENTRIES + 2u;
    while (chunk && chunk != snapshot->tail && full_before_tail < bound) {
        chunk = chunk->next;
        ++full_before_tail;
    }
    if (chunk != snapshot->tail) return 0u;
    {
        size_t full_entries =
            full_before_tail * RIN_PTHREAD_ATFORK_CHUNK_ENTRIES;
        size_t fill = snapshot->count > full_entries
            ? snapshot->count - full_entries : 0u;
        return fill > RIN_PTHREAD_ATFORK_CHUNK_ENTRIES
            ? RIN_PTHREAD_ATFORK_CHUNK_ENTRIES : fill;
    }
}

void __rin_pthread_atfork_prepare(
    __rin_pthread_atfork_snapshot* snapshot) {
    RinPthreadAtforkChunk* chunk;
    size_t fill;
    if (!snapshot) return;
    rin_pthread_atfork_lock();
    snapshot->count =
        __atomic_load_n(&g_rin_pthread_atfork_count, __ATOMIC_ACQUIRE);
    snapshot->tail =
        __atomic_load_n(&g_rin_pthread_atfork_tail, __ATOMIC_ACQUIRE);
    rin_pthread_atfork_unlock();
    /* Handlers run without the lock so a prepare handler may register.
     * Later registrations land after the snapshot tail and are ignored. */
    fill = rin_pthread_atfork_tail_fill(snapshot);
    for (chunk = snapshot->tail; chunk != NULL; chunk = chunk->prev) {
        size_t index = (chunk == snapshot->tail)
            ? fill : RIN_PTHREAD_ATFORK_CHUNK_ENTRIES;
        while (index != 0u) {
            void (*handler)(void) = chunk->entries[--index].prepare;
            if (handler) handler();
        }
    }
}

static void rin_pthread_atfork_dispatch_forward(
    const __rin_pthread_atfork_snapshot* snapshot, int child) {
    RinPthreadAtforkChunk* chunk = &g_rin_pthread_atfork_head;
    size_t fill;
    size_t bound;
    size_t walked = 0u;
    if (!snapshot || !snapshot->tail) return;
    fill = rin_pthread_atfork_tail_fill(snapshot);
    bound = snapshot->count / RIN_PTHREAD_ATFORK_CHUNK_ENTRIES + 2u;
    while (chunk && walked < bound) {
        size_t entries = (chunk == snapshot->tail)
            ? fill : RIN_PTHREAD_ATFORK_CHUNK_ENTRIES;
        size_t index;
        int done = chunk == snapshot->tail;
        for (index = 0u; index < entries; ++index) {
            void (*handler)(void) = child ? chunk->entries[index].child
                                          : chunk->entries[index].parent;
            if (handler) handler();
        }
        if (done) break;
        chunk = chunk->next;
        ++walked;
    }
}

void __rin_pthread_atfork_parent(
    const __rin_pthread_atfork_snapshot* snapshot) {
    rin_pthread_atfork_dispatch_forward(snapshot, 0);
}

void __rin_pthread_atfork_child(
    const __rin_pthread_atfork_snapshot* snapshot) {
    rin_pthread_atfork_dispatch_forward(snapshot, 1);
}
