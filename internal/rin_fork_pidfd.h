/* SPDX-License-Identifier: MIT */
/* Shared fork/pidfd frontend used without pulling in the full unistd graph. */

#ifndef RIN_INTERNAL_FORK_PIDFD_H
#define RIN_INTERNAL_FORK_PIDFD_H

#include "../errno.h"
#include "../limits.h"
#include "../sys/syscall.h"
#include "../sys/types.h"
#include "pthread_atfork.h"
#include "rin_environment_runtime.h"

/* A caller that already owns the unistd syscall hooks keeps them here too;
 * standalone users fall back to the public syscall primitives. */
#ifndef _RIN_FORK_PIDFD_SYSCALL0
#ifdef _RIN_UNISTD_SYSCALL0
#define _RIN_FORK_PIDFD_SYSCALL0(number) _RIN_UNISTD_SYSCALL0(number)
#else
#define _RIN_FORK_PIDFD_SYSCALL0(number) _syscall0((uintptr_t)(number))
#endif
#endif
#ifndef _RIN_FORK_PIDFD_SYSCALL1
#ifdef _RIN_UNISTD_SYSCALL1
#define _RIN_FORK_PIDFD_SYSCALL1(number, argument1) \
    _RIN_UNISTD_SYSCALL1((number), (argument1))
#else
#define _RIN_FORK_PIDFD_SYSCALL1(number, argument1) \
    _syscall1((uintptr_t)(number), (uintptr_t)(argument1))
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif
#if !defined(MIDL_PASS)
extern void rin_user_allocator_after_fork_child(void)
    __attribute__((weak));
extern void rin_user_allocator_before_fork(void) __attribute__((weak));
extern void rin_user_allocator_after_fork_parent(void)
    __attribute__((weak));
extern int rinfs_fork_hooks_prepare(void) __attribute__((weak));
extern int rinfs_fork_hooks_complete(int fork_succeeded)
    __attribute__((weak));
extern int rinfs_fork_prepare(void) __attribute__((weak));
extern int rinfs_fork_complete(int fork_succeeded) __attribute__((weak));
#endif
#ifdef __cplusplus
}
#endif

static inline intptr_t __rin_fork_pidfd_result(intptr_t result) {
    result = __rin_syscall_posixize(result);
    if (result < 0) {
        if (result != -1) errno = EIO;
        return -1;
    }
    return result;
}

static inline void __rin_fork_pidfd_child_exit(void) {
    (void)_RIN_FORK_PIDFD_SYSCALL1(SYS_EXIT, (uintptr_t)127u);
    for (;;) {}
}

static inline int __rin_fork_complete_local(
    int fork_succeeded, int rinfs_prepared, int hooks_prepared) {
    int result = 0;
    if (rinfs_prepared && rinfs_fork_complete != NULL) {
        int complete_result = rinfs_fork_complete(fork_succeeded);
        if (complete_result != 0) result = complete_result;
    }
    if (hooks_prepared && rinfs_fork_hooks_complete != NULL) {
        int complete_result = rinfs_fork_hooks_complete(fork_succeeded);
        if (complete_result != 0 && result == 0) result = complete_result;
    }
    return result;
}

static inline pid_t __rin_fork_with_pidfd(int* pidfd_out) {
#if !defined(MIDL_PASS)
    int allocator_fork_prepared = 0;
    int fork_hooks_prepared = 0;
    int rinfs_fork_prepared = 0;
#if !defined(RINSDK_SPLIT_BUILD)
    int atfork_handlers_prepared = 0;
    __rin_pthread_atfork_snapshot atfork_snapshot = {0u};
#endif
#endif
    intptr_t result;
    if (pidfd_out != NULL) *pidfd_out = -1;
#if !defined(MIDL_PASS)
#if !defined(RINSDK_SPLIT_BUILD)
    if (__rin_pthread_atfork_prepare != NULL) {
        __rin_pthread_atfork_prepare(&atfork_snapshot);
        atfork_handlers_prepared = 1;
    }
#endif
    if (rinfs_fork_hooks_prepare != NULL &&
        rinfs_fork_hooks_complete != NULL) {
        if (rinfs_fork_hooks_prepare() != 0) {
#if !defined(RINSDK_SPLIT_BUILD)
            if (atfork_handlers_prepared &&
                __rin_pthread_atfork_parent != NULL)
                __rin_pthread_atfork_parent(&atfork_snapshot);
#endif
            errno = EBUSY;
            return -1;
        }
        fork_hooks_prepared = 1;
    }
    if (rinfs_fork_prepare != NULL && rinfs_fork_complete != NULL) {
        if (rinfs_fork_prepare() != 0) {
            int complete_result = __rin_fork_complete_local(
                0, rinfs_fork_prepared, fork_hooks_prepared);
#if !defined(RINSDK_SPLIT_BUILD)
            if (atfork_handlers_prepared &&
                __rin_pthread_atfork_parent != NULL)
                __rin_pthread_atfork_parent(&atfork_snapshot);
#endif
            errno = complete_result != 0 ? EIO : EBUSY;
            return -1;
        }
        rinfs_fork_prepared = 1;
    }
    if (rin_user_allocator_before_fork != NULL) {
        rin_user_allocator_before_fork();
        allocator_fork_prepared = 1;
    }
#endif
    result = __rin_fork_pidfd_result(pidfd_out != NULL
        ? _RIN_FORK_PIDFD_SYSCALL1(SYS_FORK_PIDFD, (uintptr_t)pidfd_out)
        : _RIN_FORK_PIDFD_SYSCALL0(SYS_FORK));
    if (result < 0) {
#if !defined(MIDL_PASS)
        int fork_errno = errno;
        int complete_result = __rin_fork_complete_local(
            0, rinfs_fork_prepared, fork_hooks_prepared);
        if (allocator_fork_prepared &&
            rin_user_allocator_after_fork_parent != NULL)
            rin_user_allocator_after_fork_parent();
#if !defined(RINSDK_SPLIT_BUILD)
        if (atfork_handlers_prepared &&
            __rin_pthread_atfork_parent != NULL)
            __rin_pthread_atfork_parent(&atfork_snapshot);
#endif
        errno = complete_result != 0 ? EIO : fork_errno;
#endif
        return -1;
    }
#if !defined(MIDL_PASS)
#if !defined(RINSDK_SPLIT_BUILD)
    if (result == 0 && __rin_pthread_atfork_child_reset != NULL)
        __rin_pthread_atfork_child_reset();
#endif
#endif
    if ((uintptr_t)result > (uintptr_t)INT_MAX) {
#if !defined(MIDL_PASS)
        int complete_result = __rin_fork_complete_local(
            1, rinfs_fork_prepared, fork_hooks_prepared);
        if (allocator_fork_prepared &&
            rin_user_allocator_after_fork_parent != NULL)
            rin_user_allocator_after_fork_parent();
#if !defined(RINSDK_SPLIT_BUILD)
        if (atfork_handlers_prepared &&
            __rin_pthread_atfork_parent != NULL)
            __rin_pthread_atfork_parent(&atfork_snapshot);
#endif
        errno = complete_result != 0 ? EIO : EOVERFLOW;
#else
        errno = EOVERFLOW;
#endif
        return -1;
    }
    if (result > 0 && pidfd_out != NULL && *pidfd_out < 0) {
        errno = EIO;
        return -1;
    }
#if !defined(MIDL_PASS)
    {
        int complete_result = __rin_fork_complete_local(
            1, rinfs_fork_prepared, fork_hooks_prepared);
        if (result == 0 && complete_result != 0)
            __rin_fork_pidfd_child_exit();
        if (result == 0 && rin_user_allocator_after_fork_child != NULL)
            rin_user_allocator_after_fork_child();
        else if (allocator_fork_prepared &&
                 rin_user_allocator_after_fork_parent != NULL)
            rin_user_allocator_after_fork_parent();
#if !defined(RINSDK_SPLIT_BUILD)
        if (atfork_handlers_prepared) {
            if (result == 0 && __rin_pthread_atfork_child != NULL)
                __rin_pthread_atfork_child(&atfork_snapshot);
            else if (result != 0 && __rin_pthread_atfork_parent != NULL)
                __rin_pthread_atfork_parent(&atfork_snapshot);
        }
#endif
        if (result != 0 && complete_result != 0) errno = EIO;
    }
#endif
    return (pid_t)result;
}

#endif /* RIN_INTERNAL_FORK_PIDFD_H */
