/* SPDX-License-Identifier: MIT */
#ifndef RIN_THREAD_DETACH_POLICY_H
#define RIN_THREAD_DETACH_POLICY_H

#include "stdint.h"

enum rin_thread_detach_outcome {
    RIN_THREAD_DETACH_INVALID = -1,
    RIN_THREAD_DETACH_ACTIVE = 0,
    RIN_THREAD_DETACH_REAPED = 1
};

/* SYS_THREAD_DETACH has exactly two successful results: zero means that the
 * live thread is now detached, while one means that a terminated thread was
 * detached and reaped synchronously.  Never accept other positive values as
 * success; doing so would lose ownership of the userspace stack record. */
static inline enum rin_thread_detach_outcome
rin_thread_detach_classify(intptr_t result)
{
    if (result == 0) return RIN_THREAD_DETACH_ACTIVE;
    if (result == 1) return RIN_THREAD_DETACH_REAPED;
    return RIN_THREAD_DETACH_INVALID;
}

#endif /* RIN_THREAD_DETACH_POLICY_H */
