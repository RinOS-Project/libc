/* SPDX-License-Identifier: MIT */
#ifndef RIN_LIBC_PTHREAD_FUTEX_POLICY_H
#define RIN_LIBC_PTHREAD_FUTEX_POLICY_H

#include "errno.h"
#include "stdint.h"

/* The libc futex wrapper returns -1 and sets errno for a valid kernel error.
 * FUTEX_WAIT succeeds only with zero; EAGAIN/EINTR mean retry the predicate. */
static inline int rin_pthread_futex_wait_error(intptr_t result,
                                                int error_number) {
    if (result == 0) return 0;
    if (result != -1) return EIO;
    if (error_number == EAGAIN || error_number == EINTR) return 0;
    if (error_number <= 0 || error_number > 4095) return EIO;
    return error_number;
}

/* FUTEX_WAKE returns a non-negative wake count on success. */
static inline int rin_pthread_futex_wake_error(intptr_t result,
                                                int error_number) {
    if (result >= 0) return 0;
    if (result != -1 || error_number <= 0 || error_number > 4095) return EIO;
    return error_number;
}

#endif
