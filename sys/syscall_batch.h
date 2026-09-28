/* SPDX-License-Identifier: MIT */
/* Bounded native syscall batch frontend. */

#ifndef _SYS_RIN_SYSCALL_BATCH_H
#define _SYS_RIN_SYSCALL_BATCH_H

#include "syscall.h"
#include "../errno.h"
#include "../limits.h"
#include <rin/contract_abi.h>
#include <rin/syscall_abi.h>

#ifndef RIN_SYSCALL_BATCH_MAX_ITEMS
#define RIN_SYSCALL_BATCH_MAX_ITEMS 16u
#endif

static inline int rin_syscall_batch(RinSyscallBatchV1* request)
{
    intptr_t result;

    if (request == 0 ||
        request->struct_size != sizeof(*request) ||
        request->version != RIN_CONTRACT_STRUCT_VERSION_1 ||
        request->flags != 0u || request->item_count == 0u ||
        request->item_count > RIN_SYSCALL_BATCH_MAX_ITEMS ||
        request->reserved0 != 0u || request->items == 0u ||
        request->item_stride != sizeof(RinSyscallBatchItemV1) ||
        request->reserved1 != 0u) {
        errno = EINVAL;
        return -1;
    }
    result = _syscall1((uintptr_t)SYS_SYSCALL_BATCH,
                       (uintptr_t)(void*)request);
    result = __rin_syscall_posixize(result);
    if (result < 0) {
        errno = result >= -4095 ? (int)-result : EIO;
        return -1;
    }
    if (result != 0) {
        errno = result > INT_MAX ? EOVERFLOW : EIO;
        return -1;
    }
    return 0;
}

#endif /* _SYS_RIN_SYSCALL_BATCH_H */
