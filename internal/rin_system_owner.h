/* SPDX-License-Identifier: MIT */
/* Shared bounded command-processor owner for C and C++ stdlib surfaces. */

#ifndef RIN_SYSTEM_OWNER_H
#define RIN_SYSTEM_OWNER_H

#include "../limits.h"
#include "../signal.h"
#include "../pthread.h"
#include "../sys/wait.h"
#include "pthread_cancel_point.h"
#include "rin_environment_runtime.h"

#if !defined(RIN_LIBC_PTHREAD_HEADER)
typedef struct RinSystemOwnerCleanupBuffer {
    struct RinSystemOwnerCleanupBuffer* next;
    void (*routine)(void*);
    void* argument;
    int active;
} RinSystemOwnerCleanupBuffer;
#endif
#if defined(__GNUC__) || defined(__clang__)
#ifdef __cplusplus
extern "C" {
#endif
#if defined(RIN_LIBC_PTHREAD_HEADER)
extern int pthread_setcanceltype(int type, int* oldtype)
    __attribute__((weak));
extern void __rin_pthread_cleanup_push_node(
    __rin_pthread_cleanup_buffer* buffer, void (*routine)(void*),
    void* argument) __attribute__((weak));
#else
extern int pthread_setcanceltype(int type, int* oldtype)
    __attribute__((weak));
extern void __rin_pthread_cleanup_push_node(
    RinSystemOwnerCleanupBuffer* buffer, void (*routine)(void*),
    void* argument) __attribute__((weak));
#endif
extern void __rin_pthread_cleanup_pop_node(int execute)
    __attribute__((weak));
#ifdef __cplusplus
}
#endif
#endif

#ifndef RIN_STDLIB_SYSTEM_COMMAND_MAX
#define RIN_STDLIB_SYSTEM_COMMAND_MAX 4095u
#endif

#ifndef RIN_SYSTEM_OWNER_ENVIRON_DECLARED
#define RIN_SYSTEM_OWNER_ENVIRON_DECLARED 1
#ifdef __cplusplus
extern "C" {
#endif
extern char** environ;
#ifdef __cplusplus
}
#endif
#endif

/* Process-wide signal dispositions require one lock shared by every
 * translation unit that contains the inline system() frontend. */
#ifdef __cplusplus
extern "C" {
#endif
int __rin_system_disposition_lock(void);
int __rin_system_disposition_unlock(void);
#ifdef __cplusplus
}
#endif

#ifndef _RIN_STDLIB_SYSTEM_DISPOSITION_LOCK
#define _RIN_STDLIB_SYSTEM_DISPOSITION_LOCK() \
    __rin_system_disposition_lock()
#endif
#ifndef _RIN_STDLIB_SYSTEM_DISPOSITION_UNLOCK
#define _RIN_STDLIB_SYSTEM_DISPOSITION_UNLOCK() \
    __rin_system_disposition_unlock()
#endif

/* The C header supplies these seams before including this owner.  cstdlib.h
 * includes the same defaults for freestanding C++ translation units so both
 * public surfaces exercise one lifecycle contract. */
#ifndef _RIN_STDLIB_SYSTEM_FORK
#if defined(RIN_FREESTANDING) && RIN_FREESTANDING
#define _RIN_STDLIB_SYSTEM_FORK() _syscall0((uintptr_t)SYS_FORK)
#else
#define _RIN_STDLIB_SYSTEM_FORK() ((intptr_t)-ENOSYS)
#endif
#endif
#ifndef _RIN_STDLIB_SYSTEM_EXECVE
#if defined(RIN_FREESTANDING) && RIN_FREESTANDING
#define _RIN_STDLIB_SYSTEM_EXECVE(pathname, arguments, environment) \
    _syscall3((uintptr_t)SYS_EXEC, (uintptr_t)(pathname), \
              (uintptr_t)(arguments), (uintptr_t)(environment))
#else
#define _RIN_STDLIB_SYSTEM_EXECVE(pathname, arguments, environment) \
    ((void)(pathname), (void)(arguments), (void)(environment), \
     ((intptr_t)-ENOSYS))
#endif
#endif
#ifndef _RIN_STDLIB_SYSTEM_WAITPID
#if defined(RIN_FREESTANDING) && RIN_FREESTANDING
#define _RIN_STDLIB_SYSTEM_WAITPID(pid, status, options) \
    waitpid((pid_t)(pid), (status), (int)(options))
#else
#define _RIN_STDLIB_SYSTEM_WAITPID(pid, status, options) \
    ((void)(pid), (void)(status), (void)(options), ((intptr_t)-ENOSYS))
#endif
#endif
#ifndef _RIN_STDLIB_SYSTEM_WAITPID_RAW
#if defined(RIN_FREESTANDING) && RIN_FREESTANDING
#define _RIN_STDLIB_SYSTEM_WAITPID_RAW(pid, status, options) \
    _syscall3((uintptr_t)SYS_WAIT, (uintptr_t)(pid), (uintptr_t)(status), \
              (uintptr_t)(options))
#else
#define _RIN_STDLIB_SYSTEM_WAITPID_RAW(pid, status, options) \
    ((void)(pid), (void)(status), (void)(options), ((intptr_t)-ENOSYS))
#endif
#endif
#ifndef _RIN_STDLIB_SYSTEM_KILL_RAW
#if defined(RIN_FREESTANDING) && RIN_FREESTANDING
#define _RIN_STDLIB_SYSTEM_KILL_RAW(pid, signal_number) \
    _syscall2((uintptr_t)SYS_KILL, (uintptr_t)(pid), \
              (uintptr_t)(signal_number))
#else
#define _RIN_STDLIB_SYSTEM_KILL_RAW(pid, signal_number) \
    ((void)(pid), (void)(signal_number), ((intptr_t)-ENOSYS))
#endif
#endif
#ifndef _RIN_STDLIB_SYSTEM_SIGACTION
#define _RIN_STDLIB_SYSTEM_SIGACTION(signum, action, old_action) \
    sigaction((signum), (action), (old_action))
#endif
#ifndef _RIN_STDLIB_SYSTEM_SIGPROCMASK
#define _RIN_STDLIB_SYSTEM_SIGPROCMASK(how, set, old_set) \
    sigprocmask((how), (set), (old_set))
#endif
#ifndef _RIN_STDLIB_SYSTEM_EXIT
#if defined(RIN_FREESTANDING) && RIN_FREESTANDING
#define _RIN_STDLIB_SYSTEM_EXIT(status) do { \
    (void)_syscall1((uintptr_t)SYS_EXIT, (uintptr_t)(intptr_t)(status)); \
    for (;;) {} \
} while (0)
#else
#define _RIN_STDLIB_SYSTEM_EXIT(status) ((void)(status))
#endif
#endif
#ifndef _RIN_STDLIB_SYSTEM_AVAILABLE
#define _RIN_STDLIB_SYSTEM_AVAILABLE() 0
#endif

typedef struct RinSystemOwnerCleanup {
    intptr_t child_pid;
    int child_active;
    int system_lock_held;
    int interrupt_ignored;
    int quit_ignored;
    int child_mask_blocked;
    struct sigaction old_interrupt_action;
    struct sigaction old_quit_action;
    sigset_t old_mask;
} RinSystemOwnerCleanup;

static inline int _rin_system_owner_terminate_child(
    RinSystemOwnerCleanup* cleanup) {
    intptr_t result;
    int status = 0;
    if (!cleanup || !cleanup->child_active) return 0;
    do {
        result = __rin_syscall_posixize((intptr_t)
            _RIN_STDLIB_SYSTEM_WAITPID_RAW(
                cleanup->child_pid, &status, WNOHANG));
    } while (result == -1 && errno == EINTR);
    if (result == cleanup->child_pid ||
        (result == -1 && errno == ECHILD)) {
        cleanup->child_active = 0;
        return 0;
    }
    (void)__rin_syscall_posixize((intptr_t)_RIN_STDLIB_SYSTEM_KILL_RAW(
        cleanup->child_pid, SIGKILL));
    for (;;) {
        result = __rin_syscall_posixize((intptr_t)
            _RIN_STDLIB_SYSTEM_WAITPID_RAW(
                cleanup->child_pid, &status, 0u));
        if (result == cleanup->child_pid ||
            (result == -1 && errno == ECHILD)) {
            cleanup->child_active = 0;
            return 0;
        }
        if (result == -1 && errno == EINTR) continue;
        return -1;
    }
}

static inline int _rin_system_owner_restore_parent(
    RinSystemOwnerCleanup* cleanup) {
    int error = 0;
    if (!cleanup) return EINVAL;
    if (cleanup->quit_ignored) {
        if (_RIN_STDLIB_SYSTEM_SIGACTION(
                SIGQUIT, &cleanup->old_quit_action, NULL) != 0)
            error = errno;
        cleanup->quit_ignored = 0;
    }
    if (cleanup->interrupt_ignored) {
        if (_RIN_STDLIB_SYSTEM_SIGACTION(
                SIGINT, &cleanup->old_interrupt_action, NULL) != 0 &&
            error == 0)
            error = errno;
        cleanup->interrupt_ignored = 0;
    }
    if (cleanup->child_mask_blocked) {
        if (_RIN_STDLIB_SYSTEM_SIGPROCMASK(
                SIG_SETMASK, &cleanup->old_mask, NULL) != 0 &&
            error == 0)
            error = errno;
        cleanup->child_mask_blocked = 0;
    }
    if (cleanup->system_lock_held) {
        int unlock_error;
        cleanup->system_lock_held = 0;
        unlock_error = _RIN_STDLIB_SYSTEM_DISPOSITION_UNLOCK();
        if (unlock_error != 0 && error == 0) error = unlock_error;
    }
    return error;
}

static inline void _rin_system_owner_cancel_cleanup(void* argument) {
    RinSystemOwnerCleanup* cleanup = (RinSystemOwnerCleanup*)argument;
    int saved_errno = errno;
    (void)_rin_system_owner_terminate_child(cleanup);
    (void)_rin_system_owner_restore_parent(cleanup);
    errno = saved_errno;
}

static inline int _rin_system_owner(const char* command) {
    if (!command) {
        intptr_t available = __rin_syscall_posixize(
            (intptr_t)_RIN_STDLIB_SYSTEM_AVAILABLE());
        if (available < 0) {
            if (available != -1) errno = EIO;
            return -1;
        }
        if (available > 1) {
            errno = EIO;
            return -1;
        }
        return (int)available;
    }

    {
        size_t command_length = 0u;
        intptr_t fork_result;
        intptr_t wait_result;
        int status = 0;
        int wait_error = 0;
        int signal_error = 0;
        RinSystemOwnerCleanup cleanup = {0};
        int old_cancel_type = PTHREAD_CANCEL_DEFERRED;
        int cancel_type_changed = 0;
        int cleanup_registered = 0;
#if defined(RIN_LIBC_PTHREAD_HEADER)
        __rin_pthread_cleanup_buffer cleanup_buffer;
#else
        RinSystemOwnerCleanupBuffer cleanup_buffer;
#endif
        struct sigaction ignore_action;
        sigset_t child_mask = 0;
        char** environment;
        int result = -1;
#if defined(RIN_FREESTANDING) && defined(RIN_USERSPACE)
        unsigned int environment_epoch = 0u;
#endif
        char* shell_arguments[4];

        while (command_length <= RIN_STDLIB_SYSTEM_COMMAND_MAX &&
               command[command_length] != '\0')
            ++command_length;
        if (command_length > RIN_STDLIB_SYSTEM_COMMAND_MAX) {
            errno = E2BIG;
            return -1;
        }

#if (defined(__GNUC__) || defined(__clang__))
        if (pthread_setcanceltype != 0 &&
            __rin_pthread_cleanup_push_node != 0 &&
            __rin_pthread_cleanup_pop_node != 0) {
            /* Route asynchronous requests through waitpid's cancellation
             * point while the cleanup handler owns the child and signal
             * state, then restore the caller's original cancellation type. */
            wait_error = pthread_setcanceltype(
                PTHREAD_CANCEL_DEFERRED, &old_cancel_type);
            if (wait_error != 0) {
                errno = wait_error;
                return -1;
            }
            cancel_type_changed = 1;
            __rin_pthread_cleanup_push_node(
                &cleanup_buffer, _rin_system_owner_cancel_cleanup, &cleanup);
            cleanup_registered = 1;
        }
#endif
        wait_error = _RIN_STDLIB_SYSTEM_DISPOSITION_LOCK();
        if (wait_error != 0) goto system_transaction_finish;
        cleanup.system_lock_held = 1;

        if (_RIN_STDLIB_SYSTEM_SIGACTION(SIGINT, NULL,
                                        &cleanup.old_interrupt_action) != 0) {
            wait_error = errno;
            goto system_transaction_finish;
        }
        if (_RIN_STDLIB_SYSTEM_SIGACTION(SIGQUIT, NULL,
                                        &cleanup.old_quit_action) != 0) {
            wait_error = errno;
            goto system_transaction_finish;
        }
        (void)sigaddset(&child_mask, SIGCHLD);
        if (_RIN_STDLIB_SYSTEM_SIGPROCMASK(
                SIG_BLOCK, &child_mask, &cleanup.old_mask) != 0) {
            wait_error = errno;
            goto system_transaction_finish;
        }
        cleanup.child_mask_blocked = 1;
        ignore_action.sa_handler = SIG_IGN;
        ignore_action.sa_mask = 0;
        ignore_action.sa_flags = 0;
        ignore_action.sa_restorer = NULL;
        if (_RIN_STDLIB_SYSTEM_SIGACTION(SIGINT, &ignore_action, NULL) != 0) {
            wait_error = errno;
            goto system_transaction_finish;
        }
        cleanup.interrupt_ignored = 1;
        if (_RIN_STDLIB_SYSTEM_SIGACTION(SIGQUIT, &ignore_action, NULL) != 0) {
            wait_error = errno;
            goto system_transaction_finish;
        }
        cleanup.quit_ignored = 1;
#if defined(RIN_FREESTANDING) && defined(RIN_USERSPACE)
        environment = __rin_env_read_begin(&environment_epoch);
#else
        environment = environ;
#endif
        fork_result = __rin_syscall_posixize(
            (intptr_t)_RIN_STDLIB_SYSTEM_FORK());
#if defined(RIN_FREESTANDING) && defined(RIN_USERSPACE)
        if (fork_result != 0) __rin_env_read_end(environment_epoch);
#endif
        if (fork_result < 0) {
            if (fork_result != -1) errno = EIO;
            wait_error = errno;
            goto system_transaction_finish;
        }
        if (fork_result == 0) {
            intptr_t exec_result;
            int exec_errno;
            int restore_error = _rin_system_owner_restore_parent(&cleanup);
            if (restore_error != 0) {
                exec_errno = restore_error;
#if defined(RIN_FREESTANDING) && defined(RIN_USERSPACE)
                __rin_env_read_end(environment_epoch);
#endif
                _RIN_STDLIB_SYSTEM_EXIT(127);
                errno = exec_errno;
                result = 127 << 8;
                goto system_transaction_finish;
            }
            shell_arguments[0] = (char*)"sh";
            shell_arguments[1] = (char*)"-c";
            shell_arguments[2] = (char*)command;
            shell_arguments[3] = NULL;
            exec_result = __rin_syscall_posixize((intptr_t)
                _RIN_STDLIB_SYSTEM_EXECVE("/bin/sh", shell_arguments,
                                          environment));
            if (exec_result >= 0) {
                errno = EIO;
                exec_result = -1;
            }
            exec_errno = errno;
#if defined(RIN_FREESTANDING) && defined(RIN_USERSPACE)
            __rin_env_read_end(environment_epoch);
#endif
            _RIN_STDLIB_SYSTEM_EXIT(127);
            /* A test owner may return from the exit seam.  Preserve the
             * shell's conventional 127 status instead of exposing success. */
            errno = exec_errno;
            result = 127 << 8;
            goto system_transaction_finish;
        }
        cleanup.child_pid = fork_result;
        cleanup.child_active = 1;
        if (fork_result > (intptr_t)INT_MAX) {
            wait_error = EOVERFLOW;
            goto system_transaction_finish;
        }
        do {
            wait_result = __rin_syscall_posixize((intptr_t)
                _RIN_STDLIB_SYSTEM_WAITPID(fork_result, &status, 0u));
            if (wait_result >= 0) break;
            if (wait_result != -1) errno = EIO;
            if (errno != EINTR) {
                wait_error = errno;
                break;
            }
        } while (1);
        if (wait_error == 0 && wait_result == fork_result) {
            cleanup.child_active = 0;
            result = status;
        } else if (wait_error == 0) {
            wait_error = EIO;
        }

system_transaction_finish:
        if (cleanup.child_active && wait_error != 0)
            (void)_rin_system_owner_terminate_child(&cleanup);
        signal_error = _rin_system_owner_restore_parent(&cleanup);
#if defined(__GNUC__) || defined(__clang__)
        if (cleanup_registered) __rin_pthread_cleanup_pop_node(0);
        if (cancel_type_changed && pthread_setcanceltype != 0) {
            int cancel_error = pthread_setcanceltype(old_cancel_type, NULL);
            if (cancel_error != 0 && signal_error == 0)
                signal_error = cancel_error;
        }
#endif
        __rin_pthread_cancel_point_if_linked();
        if (signal_error != 0) {
            errno = signal_error;
            return -1;
        }
        if (wait_error != 0) {
            errno = wait_error;
            return -1;
        }
        return result;

    }
}

#endif /* RIN_SYSTEM_OWNER_H */
