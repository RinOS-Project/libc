/*
 * RinOS libc - atomic wait/notify helpers
 *
 * The C11 atomic API predates atomic_wait/notify.  These helpers provide the
 * fixed-width wait primitive needed by runtimes without exposing a Linux
 * futex number or requiring a host pthread condition variable.
 */

#ifndef RIN_LIBC_ATOMIC_WAIT_H
#define RIN_LIBC_ATOMIC_WAIT_H

#include "stdint.h"
#include "errno.h"
#include "time.h"
#include "sys/syscall.h"
#include "linux/futex.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Wait until *address differs from expected, or until the kernel wakes this
 * waiter.  A value change, spurious wake, EAGAIN, or EINTR is a successful
 * return because callers must always re-check their predicate.  A malformed
 * timeout or a kernel error is reported as -1 with errno set.
 *
 * The timeout is relative and is consumed by the RinOS FUTEX_WAIT ABI.  NULL
 * means no timeout.  The address must be naturally aligned for uint32_t and
 * remain valid for the duration of the call.
 */
static inline int rin_atomic_wait_u32(volatile uint32_t* address,
                                      uint32_t expected,
                                      const struct timespec* relative_timeout) {
    if (!address || (relative_timeout &&
                     (relative_timeout->tv_sec < 0 ||
                      relative_timeout->tv_nsec < 0 ||
                      relative_timeout->tv_nsec >= 1000000000L))) {
        errno = EINVAL;
        return -1;
    }

    long result = syscall(SYS_futex, address, FUTEX_WAIT,
                          (int32_t)expected, relative_timeout, NULL, 0);
    if (result == 0 || errno == EAGAIN || errno == EINTR) return 0;
    return -1;
}

/* Wake at most one waiter currently blocked on address. */
static inline int rin_atomic_notify_one_u32(volatile uint32_t* address) {
    if (!address) {
        errno = EINVAL;
        return -1;
    }
    long result = syscall(SYS_futex, address, FUTEX_WAKE, 1, NULL, NULL, 0);
    return result < 0 ? -1 : (int)result;
}

/* Wake all waiters currently blocked on address. */
static inline int rin_atomic_notify_all_u32(volatile uint32_t* address) {
    if (!address) {
        errno = EINVAL;
        return -1;
    }
    long result = syscall(SYS_futex, address, FUTEX_WAKE, 0x7fffffff,
                          NULL, NULL, 0);
    return result < 0 ? -1 : (int)result;
}

#ifdef __cplusplus
}
#endif

#endif /* RIN_LIBC_ATOMIC_WAIT_H */
