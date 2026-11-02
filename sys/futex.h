/* SPDX-License-Identifier: MIT */
#ifndef _SYS_FUTEX_H
#define _SYS_FUTEX_H

#include <linux/futex.h>
#include <time.h>
#include <sys/syscall.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Wait until one entry in `waiters` is woken.  The return value is the
 * zero-based index of that entry, or -1 with errno set on failure. */
static inline int futex_waitv(struct futex_waitv* waiters,
                              unsigned int waiter_count,
                              unsigned int flags,
                              const struct timespec* timeout,
                              clockid_t clock_id) {
    return (int)syscall(SYS_futex_waitv, waiters, waiter_count, flags,
                        timeout, clock_id);
}

#ifdef __cplusplus
}
#endif

#endif /* _SYS_FUTEX_H */
