/* SPDX-License-Identifier: MIT */
#ifndef RIN_LIBC_INTERNAL_PTHREAD_ATFORK_H
#define RIN_LIBC_INTERNAL_PTHREAD_ATFORK_H

#include "stddef.h"

/* Registrations live in an append-only chunk list so the table grows with
 * demand instead of failing past a fixed cap.  The first chunk is static;
 * overflow chunks are allocated on demand and never freed, which keeps
 * fork-time dispatch safe: a snapshot taken under the registry lock stays
 * valid even if another thread registers (or forks) concurrently. */
#define RIN_PTHREAD_ATFORK_CHUNK_ENTRIES 32u

typedef struct RinPthreadAtforkEntry {
    void (*prepare)(void);
    void (*parent)(void);
    void (*child)(void);
} RinPthreadAtforkEntry;

typedef struct RinPthreadAtforkChunk {
    struct RinPthreadAtforkChunk* next;
    struct RinPthreadAtforkChunk* prev;
    RinPthreadAtforkEntry entries[RIN_PTHREAD_ATFORK_CHUNK_ENTRIES];
} RinPthreadAtforkChunk;

typedef struct __rin_pthread_atfork_snapshot {
    size_t count;
    RinPthreadAtforkChunk* tail;
} __rin_pthread_atfork_snapshot;

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__GNUC__) || defined(__clang__)
void __rin_pthread_atfork_prepare(
    __rin_pthread_atfork_snapshot* snapshot) __attribute__((weak));
void __rin_pthread_atfork_parent(
    const __rin_pthread_atfork_snapshot* snapshot) __attribute__((weak));
void __rin_pthread_atfork_child(
    const __rin_pthread_atfork_snapshot* snapshot) __attribute__((weak));
void __rin_pthread_atfork_child_reset(void) __attribute__((weak));
#else
void __rin_pthread_atfork_prepare(
    __rin_pthread_atfork_snapshot* snapshot);
void __rin_pthread_atfork_parent(
    const __rin_pthread_atfork_snapshot* snapshot);
void __rin_pthread_atfork_child(
    const __rin_pthread_atfork_snapshot* snapshot);
void __rin_pthread_atfork_child_reset(void);
#endif

#ifdef __cplusplus
}
#endif

#endif
