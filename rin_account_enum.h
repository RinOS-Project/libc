/* SPDX-License-Identifier: MIT */

#ifndef RIN_LIBC_ACCOUNT_ENUM_H
#define RIN_LIBC_ACCOUNT_ENUM_H

#include "rin_account_compat.h"

/* Resume account enumeration after a UID.  The kernel selects the least
 * matching UID, so array compaction cannot duplicate or skip later records. */
static inline int __rin_account_query_uid_after(
    uint32_t uid, __rin_account_info_v1* info)
{
    __rin_account_query_v1 query;
    intptr_t result;
    int error;
    if (!info) return EFAULT;
    __rin_account_zero(&query, sizeof(query));
    query.struct_size = sizeof(query);
    query.version = __RIN_ACCOUNT_QUERY_VERSION;
    query.kind = RIN_ACCOUNT_QUERY_BY_UID_AFTER;
    query.uid = uid;
    __rin_account_zero(info, sizeof(*info));
    info->struct_size = sizeof(*info);
    info->version = __RIN_ACCOUNT_INFO_VERSION;
    result = _RIN_ACCOUNT_SYSCALL2(RIN_SYS_ACCOUNT_QUERY, &query, info);
    error = __rin_account_errno(result);
    if (error != 0) return error;
    return __rin_account_validate_info(info);
}

#endif /* RIN_LIBC_ACCOUNT_ENUM_H */
