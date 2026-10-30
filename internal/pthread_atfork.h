/* SPDX-License-Identifier: MIT */
#ifndef RIN_LIBC_INTERNAL_PTHREAD_ATFORK_H
#define RIN_LIBC_INTERNAL_PTHREAD_ATFORK_H

#include "stddef.h"

#define RIN_PTHREAD_ATFORK_MAX 128u

typedef struct __rin_pthread_atfork_snapshot {
    size_t count;
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
