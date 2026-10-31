/* SPDX-License-Identifier: MIT */
#if defined(__STDC_HOSTED__) && __STDC_HOSTED__ && \
    (!defined(RIN_FREESTANDING) || !(RIN_FREESTANDING)) && \
    defined(__linux__) && (defined(__GNUC__) || defined(__clang__))
#include_next <sys/capability.h>
#else
#ifndef _SYS_CAPABILITY_H
#define _SYS_CAPABILITY_H

#include "../errno.h"
#include "../linux/capability.h"
#include "syscall.h"

#ifdef __cplusplus
extern "C" {
#endif

static inline int capget(cap_user_header_t header, cap_user_data_t data)
{
    intptr_t result = (intptr_t)syscall(SYS_CAPGET, header, data);
    if (result == 0) return 0;
    if (result > 0) errno = EIO;
    return -1;
}

static inline int capset(cap_user_header_t header, const cap_user_data_t data)
{
    intptr_t result = (intptr_t)syscall(SYS_CAPSET, header, data);
    if (result == 0) return 0;
    if (result > 0) errno = EIO;
    return -1;
}

#ifdef __cplusplus
}
#endif

#endif /* _SYS_CAPABILITY_H */
#endif
