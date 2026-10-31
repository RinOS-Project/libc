/* SPDX-License-Identifier: MIT */
/* POSIX named semaphore syscall bridge. */

#include "semaphore.h"
#include "fcntl_flags.h"
#include "stdarg.h"

#ifndef _RIN_SEMAPHORE_SYSCALL1
#define _RIN_SEMAPHORE_SYSCALL1(number, argument1) \
    _syscall1((uintptr_t)(number), (uintptr_t)(argument1))
#endif
#ifndef _RIN_SEMAPHORE_SYSCALL3
#define _RIN_SEMAPHORE_SYSCALL3(number, argument1, argument2, argument3) \
    _syscall3((uintptr_t)(number), (uintptr_t)(argument1), \
              (uintptr_t)(argument2), (uintptr_t)(argument3))
#endif

static intptr_t rin_semaphore_status_result(intptr_t result)
{
    if (result < 0 && result >= -4095)
        return __rin_syscall_posixize(result);
    if (result < 0) {
        errno = EIO;
        return -1;
    }
    return result;
}

static intptr_t rin_semaphore_pointer_result(intptr_t result)
{
    if (result < 0 && result >= -4095)
        return __rin_syscall_posixize(result);
    if (result < 0) {
        errno = EIO;
        return -1;
    }
    return result;
}

static int rin_semaphore_status(intptr_t result)
{
    result = rin_semaphore_status_result(result);
    if (result == 0) return 0;
    if (result != -1) errno = EIO;
    return -1;
}

int sem_clockwait(sem_t* semaphore, clockid_t clock_id,
                  const struct timespec* abstime)
{
    return __rin_sem_clockwait_impl(semaphore, clock_id, abstime);
}

int sem_timedwait(sem_t* semaphore, const struct timespec* abstime)
{
    return sem_clockwait(semaphore, CLOCK_REALTIME, abstime);
}

sem_t* sem_open(const char* name, int oflag, ...)
{
    uint32_t kernel_flags = 0u;
    unsigned int initial_value = 0u;
    intptr_t result;

    if (!name || (oflag & ~(O_CREAT | O_EXCL)) != 0 ||
        ((oflag & O_EXCL) != 0 && (oflag & O_CREAT) == 0)) {
        errno = EINVAL;
        return SEM_FAILED;
    }

    if ((oflag & O_CREAT) != 0) {
        va_list arguments;
        mode_t ignored_mode;
        va_start(arguments, oflag);
        ignored_mode = va_arg(arguments, mode_t);
        initial_value = va_arg(arguments, unsigned int);
        va_end(arguments);
        (void)ignored_mode;
        if (initial_value > SEM_VALUE_MAX) {
            errno = EINVAL;
            return SEM_FAILED;
        }
        kernel_flags |= RIN_POSIX_SEMAPHORE_OPEN_CREAT;
    }
    if ((oflag & O_EXCL) != 0)
        kernel_flags |= RIN_POSIX_SEMAPHORE_OPEN_EXCL;

    result = rin_semaphore_pointer_result(_RIN_SEMAPHORE_SYSCALL3(
        SYS_POSIX_SEM_OPEN, name, kernel_flags, initial_value));
    if (result == -1) return SEM_FAILED;
    if (result == 0) {
        errno = EIO;
        return SEM_FAILED;
    }
    return (sem_t*)(uintptr_t)result;
}

int sem_close(sem_t* semaphore)
{
    if (!semaphore) {
        errno = EINVAL;
        return -1;
    }
    return rin_semaphore_status(_RIN_SEMAPHORE_SYSCALL1(
        SYS_POSIX_SEM_CLOSE, semaphore));
}

int sem_unlink(const char* name)
{
    if (!name) {
        errno = EINVAL;
        return -1;
    }
    return rin_semaphore_status(_RIN_SEMAPHORE_SYSCALL1(
        SYS_POSIX_SEM_UNLINK, name));
}
