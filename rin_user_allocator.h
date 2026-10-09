/* SPDX-License-Identifier: MIT */
#ifndef RIN_LIBC_USER_ALLOCATOR_H
#define RIN_LIBC_USER_ALLOCATOR_H

#include "stddef.h"
#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Supplied by the application runtime (or a host test backend).  The
 * allocator core never knows syscall numbers; this is its complete VM ABI. */
void* rin_user_allocator_backend_map(size_t size);
int rin_user_allocator_backend_unmap(void* pointer, size_t size);

/* Optional protection hook used only when debug guard pages are enabled. */
int rin_user_allocator_backend_protect(void* pointer, size_t size, int prot);

/* Transitional names for older host fixtures.  The allocator core only
 * consults these through weak compatibility hooks when map/unmap is absent. */
void* rin_user_allocator_backend_allocate(size_t size);
void rin_user_allocator_backend_release(void* pointer);

/* Production runtimes override this hook with a no-return process
 * termination/crash hand-off. Host tests may provide a returning observer. */
void rin_user_allocator_corruption(void);

/* Optional runtime stack unwinder.  It returns the number of frames written
 * to `frames`; when absent, the allocator records its direct caller only. */
size_t rin_user_allocator_capture_backtrace(uintptr_t* frames,
                                            size_t capacity);

/* A child created by fork() inherits allocator metadata but not the threads
 * that may have owned these spin locks.  The target fork frontend calls this
 * immediately in the child before user code can allocate again. */
void rin_user_allocator_after_fork_child(void);
void rin_user_allocator_before_fork(void);
void rin_user_allocator_after_fork_parent(void);

typedef struct RinUserAllocatorMetricsV1 {
    uint32_t struct_size;
    uint32_t version;
    uint64_t arena_lock_acquisitions;
    uint64_t magazine_hits;
    uint64_t magazine_refills;
    uint64_t magazine_flushes;
    uint64_t cached_bytes;
    uint64_t remote_free_enqueues;
    uint64_t remote_free_drains;
    /* Current total depth and highest lifetime depth of any one arena. */
    uint64_t remote_free_queue_depth;
    uint64_t remote_free_queue_peak_depth;
} RinUserAllocatorMetricsV1;

/* Flush live thread magazines and drain arena remote-free queues. */
void rin_user_allocator_trim(void);
/* Snapshot lock, magazine, and remote-free counters without changing state. */
int rin_user_allocator_metrics_read(RinUserAllocatorMetricsV1* metrics);

void* rin_user_allocator_malloc(size_t size);
void rin_user_allocator_free(void* pointer);
void* rin_user_allocator_realloc(void* pointer, size_t size);
size_t rin_user_allocator_usable_size(const void* pointer);

/* Copy the bounded allocation-site trace for a live allocation.  The return
 * value is the total number of recorded frames, even when `capacity` is
 * smaller than that total.  Invalid, freed, or corrupt pointers fail closed
 * with zero and invoke the corruption hook. */
size_t rin_user_allocator_get_backtrace(const void* pointer,
                                        uintptr_t* frames,
                                        size_t capacity);

#ifdef __cplusplus
}
#endif

#endif
