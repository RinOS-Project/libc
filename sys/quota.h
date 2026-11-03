/* SPDX-License-Identifier: MIT */
/* Descriptor-bound UID, GID, and project quota control. */

#if defined(__STDC_HOSTED__) && __STDC_HOSTED__ && \
    (!defined(RIN_FREESTANDING) || !(RIN_FREESTANDING)) && \
    defined(__linux__) && (defined(__GNUC__) || defined(__clang__)) && \
    !defined(_RIN_QUOTACTL_FD_SYSCALL1)
#include_next <sys/quota.h>
#else

#ifndef _SYS_QUOTA_H
#define _SYS_QUOTA_H

#include "../errno.h"
#include "../stdint.h"
#include "syscall.h"
#include <rin/contract_abi.h>
#include <rin/syscall_abi.h>

#ifndef _RIN_QUOTACTL_FD_SYSCALL1
#define _RIN_QUOTACTL_FD_SYSCALL1(number, request) \
    _syscall1((uintptr_t)(number), (uintptr_t)(request))
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* The request is both the input policy and GET result. Block usage is
 * measured in 512-byte units; IDs belong to the filesystem namespace bound
 * to fd. Unsupported filesystems return EOPNOTSUPP. */
static inline int quotactl_fd(int fd, RinQuotaCtlFdRequestV1* request) {
    RinQuotaCtlFdRequestV1 call;
    intptr_t result;

    if (fd < 0) {
        errno = EBADF;
        return -1;
    }
    if (!request) {
        errno = EFAULT;
        return -1;
    }
    if (request->struct_size != sizeof(*request) ||
        request->version != RIN_QUOTACTL_FD_ABI_VERSION ||
        (request->command != RIN_QUOTACTL_FD_COMMAND_SET &&
         request->command != RIN_QUOTACTL_FD_COMMAND_GET &&
         request->command != RIN_QUOTACTL_FD_COMMAND_SET_PROJECT_ID)) {
        errno = EINVAL;
        return -1;
    }
    call = *request;
    call.fd = fd;
    result = (intptr_t)__rin_syscall_posixize(
        _RIN_QUOTACTL_FD_SYSCALL1(RIN_SYS_QUOTACTL_FD, &call));
    if (result < 0) {
        errno = result >= -4095 ? (int)-result : EIO;
        return -1;
    }
    if (result != 0) {
        errno = EIO;
        return -1;
    }
    if (call.command == RIN_QUOTACTL_FD_COMMAND_GET) *request = call;
    return 0;
}

#ifdef __cplusplus
}
#endif

#endif /* _SYS_QUOTA_H */

#endif /* hosted Linux delegates to its native quota API */
