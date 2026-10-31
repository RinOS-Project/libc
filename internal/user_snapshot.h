/* SPDX-License-Identifier: MIT */

#ifndef RIN_LIBC_INTERNAL_USER_SNAPSHOT_H
#define RIN_LIBC_INTERNAL_USER_SNAPSHOT_H

#include "../errno.h"
#include "../stddef.h"
#include "../stdint.h"
#include "../sys/syscall.h"
#include <rin/abi.h>

/* IPC checked-copy requests use this bounded response size. Keep it in sync
 * with RIN_KERNEL_POSIX_MQ_USER_SNAPSHOT_MAX in the kernel owner. */
#define __RIN_USER_SNAPSHOT_MAX 1024u

static inline int __rin_user_snapshot(const void* source, size_t size,
                                      int string_mode, void* destination)
{
    RinSdkArgsV1 request = {0};
    intptr_t result;

    if (!source || !destination) {
        errno = EFAULT;
        return -1;
    }
    if (size == 0u || size > __RIN_USER_SNAPSHOT_MAX) {
        errno = EINVAL;
        return -1;
    }

    request.struct_size = (uint32_t)sizeof(request);
    request.version = RIN_SDK_STRUCT_VERSION_1;
    request.value[0] = (uint64_t)(uintptr_t)source;
    request.value[1] = (uint64_t)size;
    request.value[2] = string_mode ? 1u : 0u;
    result = __RIN_SYSCALL_7(
        SYS_SDK_CALL, (uintptr_t)&request, (uintptr_t)sizeof(request),
        RIN_SDK_LIBRARY_IPC, 28u, (uintptr_t)destination,
        (uintptr_t)size);

    if (result == RIN_SUCCESS) return 0;
    switch (result) {
    case RIN_ERROR_INVALID_ADDRESS: errno = EFAULT; break;
    case RIN_ERROR_LIMIT_EXCEEDED:
        errno = string_mode ? ENAMETOOLONG : EOVERFLOW;
        break;
    case RIN_ERROR_INVALID_ARGUMENT: errno = EINVAL; break;
    case RIN_ERROR_NOT_SUPPORTED: errno = ENOSYS; break;
    case RIN_ERROR_NO_MEMORY: errno = ENOMEM; break;
    case RIN_ERROR_OVERFLOW: errno = EOVERFLOW; break;
    default: errno = EIO; break;
    }
    return -1;
}

static inline int __rin_user_snapshot_string(const char* source,
                                             char* destination,
                                             size_t capacity)
{
    return __rin_user_snapshot(source, capacity, 1, destination);
}

static inline int __rin_user_snapshot_byte(const void* source,
                                           unsigned char* destination)
{
    return __rin_user_snapshot(source, sizeof(*destination), 0,
                               destination);
}

#endif /* RIN_LIBC_INTERNAL_USER_SNAPSHOT_H */
