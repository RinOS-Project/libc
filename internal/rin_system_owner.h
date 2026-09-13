/* SPDX-License-Identifier: MIT */
/* Shared bounded command-processor owner for C and C++ stdlib surfaces. */

#ifndef RIN_SYSTEM_OWNER_H
#define RIN_SYSTEM_OWNER_H

#include "../limits.h"

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
    _syscall3((uintptr_t)SYS_WAIT, (uintptr_t)(pid), (uintptr_t)(status), \
              (uintptr_t)(options))
#else
#define _RIN_STDLIB_SYSTEM_WAITPID(pid, status, options) \
    ((void)(pid), (void)(status), (void)(options), ((intptr_t)-ENOSYS))
#endif
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
        char* shell_arguments[4];

        while (command_length <= RIN_STDLIB_SYSTEM_COMMAND_MAX &&
               command[command_length] != '\0')
            ++command_length;
        if (command_length > RIN_STDLIB_SYSTEM_COMMAND_MAX) {
            errno = E2BIG;
            return -1;
        }

        fork_result = __rin_syscall_posixize(
            (intptr_t)_RIN_STDLIB_SYSTEM_FORK());
        if (fork_result < 0) {
            if (fork_result != -1) errno = EIO;
            return -1;
        }
        if (fork_result == 0) {
            intptr_t exec_result;
            int exec_errno;
            shell_arguments[0] = (char*)"sh";
            shell_arguments[1] = (char*)"-c";
            shell_arguments[2] = (char*)command;
            shell_arguments[3] = NULL;
            exec_result = __rin_syscall_posixize((intptr_t)
                _RIN_STDLIB_SYSTEM_EXECVE("/bin/sh", shell_arguments,
                                          environ));
            if (exec_result >= 0) {
                errno = EIO;
                exec_result = -1;
            }
            exec_errno = errno;
            _RIN_STDLIB_SYSTEM_EXIT(127);
            /* A test owner may return from the exit seam.  Preserve the
             * shell's conventional 127 status instead of exposing success. */
            errno = exec_errno;
            return 127 << 8;
        }
        if (fork_result > (intptr_t)INT_MAX) {
            errno = EOVERFLOW;
            return -1;
        }
        wait_result = __rin_syscall_posixize((intptr_t)
            _RIN_STDLIB_SYSTEM_WAITPID(fork_result, &status, 0u));
        if (wait_result < 0) {
            if (wait_result != -1) errno = EIO;
            return -1;
        }
        if (wait_result != fork_result) {
            errno = EIO;
            return -1;
        }
        return status;
    }
}

#endif /* RIN_SYSTEM_OWNER_H */
