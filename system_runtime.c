/* SPDX-License-Identifier: MIT */
/* Out-of-line process owner for split libc and RinSDK runtime builds. */

#define RINSDK_SPLIT_BUILD 1
#include "signal.h"
#include "stdlib.h"
#include "fcntl.h"
#include "unistd.h"
#include "rin_atomic_wait.h"
#include "internal/rin_system_owner.h"

int __rin_stdlib_system_available(void) {
#if defined(RIN_FREESTANDING) && RIN_FREESTANDING && \
    defined(RIN_USERSPACE) && RIN_USERSPACE
    const int saved_errno = errno;
    const int available =
        faccessat(AT_FDCWD, "/bin/sh", X_OK, AT_EACCESS) == 0;
    errno = saved_errno;
    return available;
#else
    return 0;
#endif
}

#if defined(RIN_FREESTANDING) && RIN_FREESTANDING && \
    defined(RIN_USERSPACE) && RIN_USERSPACE
#ifndef RIN_STDLIB_SYSTEM_LOCK_WAIT
#define RIN_STDLIB_SYSTEM_LOCK_WAIT(address, expected) \
    rin_atomic_wait_u32((address), (expected), NULL)
#endif
#ifndef RIN_STDLIB_SYSTEM_LOCK_WAKE
#define RIN_STDLIB_SYSTEM_LOCK_WAKE(address) \
    rin_atomic_notify_one_u32((address))
#endif

static volatile uint32_t g_system_disposition_lock;

int __rin_system_disposition_lock(void) {
    int saved_errno = errno;
    uint32_t expected = 0u;
    if (__atomic_compare_exchange_n(&g_system_disposition_lock, &expected,
                                    1u, 0, __ATOMIC_ACQUIRE,
                                    __ATOMIC_RELAXED)) {
        errno = saved_errno;
        return 0;
    }
    for (;;) {
        uint32_t previous = __atomic_exchange_n(
            &g_system_disposition_lock, 2u, __ATOMIC_ACQUIRE);
        if (previous == 0u) {
            errno = saved_errno;
            return 0;
        }
        if (RIN_STDLIB_SYSTEM_LOCK_WAIT(&g_system_disposition_lock, 2u) != 0) {
            int wait_error = errno;
            errno = saved_errno;
            return wait_error != 0 ? wait_error : EIO;
        }
    }
}

int __rin_system_disposition_unlock(void) {
    int saved_errno = errno;
    uint32_t previous = __atomic_exchange_n(
        &g_system_disposition_lock, 0u, __ATOMIC_RELEASE);
    if (previous == 0u) {
        errno = saved_errno;
        return EPERM;
    }
    if (previous == 2u &&
        RIN_STDLIB_SYSTEM_LOCK_WAKE(&g_system_disposition_lock) < 0) {
        int wake_error = errno;
        errno = saved_errno;
        return wake_error != 0 ? wake_error : EIO;
    }
    errno = saved_errno;
    return 0;
}
#else
int __rin_system_disposition_lock(void) { return ENOSYS; }
int __rin_system_disposition_unlock(void) { return ENOSYS; }
#endif

int system(const char* command) {
    return _rin_system_owner(command);
}
