/* SPDX-License-Identifier: MIT */
#ifndef RIN_THREAD_STACK_POLICY_H
#define RIN_THREAD_STACK_POLICY_H

#include "stddef.h"
#include "stdint.h"

#include "../../../src/shared/rin_address_space_abi.h"

/* Validate without allowing base + size to wrap.  The returned top is aligned
 * down for the x86_64 calling convention and remains inside the mapping. */
static inline int rin_thread_stack_top_checked(uintptr_t base,
                                               size_t size,
                                               uintptr_t* top_out)
{
    uint64_t start = (uint64_t)base;
    uint64_t length = (uint64_t)size;
    uint64_t top;

    if (start == 0 || length == 0 || start >= RIN_USER_STACK_TOP_EXCLUSIVE) {
        return 0;
    }
    if (length > RIN_USER_STACK_TOP_EXCLUSIVE - start) {
        return 0;
    }

    top = (start + length) & ~UINT64_C(0xF);
    if (top <= start || top > RIN_USER_STACK_TOP_EXCLUSIVE) {
        return 0;
    }
    if (top_out) {
        *top_out = (uintptr_t)top;
    }
    return 1;
}

#endif /* RIN_THREAD_STACK_POLICY_H */
