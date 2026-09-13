/* SPDX-License-Identifier: MIT */

#ifndef RIN_LIBC_ACCOUNT_COMPAT_H
#define RIN_LIBC_ACCOUNT_COMPAT_H

#include "errno.h"
#include "stddef.h"
#include "stdint.h"
#include "sys/syscall.h"

#define __RIN_CREDENTIALS_VERSION_1 1u
#define __RIN_CREDENTIALS_MAX_GROUPS 8u
#define __RIN_CREDENTIAL_SET_VERSION 1u
#define __RIN_CREDENTIAL_SET_UID 1u
#define __RIN_CREDENTIAL_SET_GID 2u
#define __RIN_SUPPLEMENTARY_GROUPS_VERSION 1u
#define __RIN_SUPPLEMENTARY_GROUPS_OPERATION_SET 1u
#define __RIN_ACCOUNT_QUERY_VERSION 1u
#define __RIN_ACCOUNT_INFO_VERSION 1u
#define __RIN_ACCOUNT_QUERY_BY_UID 1u
#define __RIN_ACCOUNT_QUERY_BY_NAME 2u
#define __RIN_ACCOUNT_QUERY_BY_INDEX 3u
#define __RIN_ACCOUNT_INFO_FLAG_ACTIVE 1u
#define __RIN_ACCOUNT_INFO_FLAG_ADMIN 2u

typedef struct __rin_credentials_v1 {
    uint32_t struct_size;
    uint32_t version;
    uint32_t uid;
    uint32_t gid;
    uint32_t effective_uid;
    uint32_t effective_gid;
    uint32_t groups[__RIN_CREDENTIALS_MAX_GROUPS];
    uint32_t group_count;
    uint32_t umask;
    uint64_t capabilities;
    uint32_t personality;
    uint32_t flags;
    uint64_t reserved[4];
} __rin_credentials_v1;

typedef struct __rin_credential_set_v1 {
    uint32_t struct_size;
    uint16_t version;
    uint16_t operation;
    uint32_t value;
    uint32_t flags;
    uint64_t reserved;
} __rin_credential_set_v1;

typedef struct __rin_supplementary_groups_v1 {
    uint32_t struct_size;
    uint16_t version;
    uint16_t operation;
    uint32_t target_uid;
    uint32_t group_count;
    uint32_t groups[__RIN_CREDENTIALS_MAX_GROUPS];
    uint32_t flags;
    uint32_t reserved0;
    uint64_t reserved;
} __rin_supplementary_groups_v1;

typedef struct __rin_account_query_v1 {
    uint32_t struct_size;
    uint16_t version;
    uint16_t kind;
    uint32_t uid;
    uint32_t flags;
    uint8_t name[32];
    uint64_t reserved;
} __rin_account_query_v1;

typedef struct __rin_account_info_v1 {
    uint32_t struct_size;
    uint16_t version;
    uint16_t flags;
    uint32_t uid;
    uint32_t gid;
    uint32_t role;
    uint32_t reserved0;
    uint8_t name[32];
    uint8_t display_name[32];
    uint8_t home_dir[256];
    uint8_t shell[64];
    uint64_t reserved[4];
} __rin_account_info_v1;

#if defined(__cplusplus)
static_assert(sizeof(__rin_credentials_v1) == 112,
              "credential wire layout changed");
static_assert(sizeof(__rin_credential_set_v1) == 24,
              "credential-set wire layout changed");
static_assert(sizeof(__rin_supplementary_groups_v1) == 64,
              "supplementary-groups wire layout changed");
static_assert(sizeof(__rin_account_query_v1) == 56,
              "account-query wire layout changed");
static_assert(sizeof(__rin_account_info_v1) == 440,
              "account-info wire layout changed");
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(__rin_credentials_v1) == 112,
               "credential wire layout changed");
_Static_assert(sizeof(__rin_credential_set_v1) == 24,
               "credential-set wire layout changed");
_Static_assert(sizeof(__rin_supplementary_groups_v1) == 64,
               "supplementary-groups wire layout changed");
_Static_assert(sizeof(__rin_account_query_v1) == 56,
               "account-query wire layout changed");
_Static_assert(sizeof(__rin_account_info_v1) == 440,
               "account-info wire layout changed");
#endif

#ifndef _RIN_CREDENTIAL_SYSCALL1
#define _RIN_CREDENTIAL_SYSCALL1(number, argument) \
    _syscall1((uintptr_t)(number), (uintptr_t)(argument))
#endif

#ifndef _RIN_ACCOUNT_SYSCALL2
#define _RIN_ACCOUNT_SYSCALL2(number, argument1, argument2) \
    _syscall2((uintptr_t)(number), (uintptr_t)(argument1), \
              (uintptr_t)(argument2))
#endif

static inline int __rin_account_string_terminated(const uint8_t* value,
                                                  size_t capacity)
{
    size_t index;
    if (!value || capacity == 0u || value[0] == 0u) return 0;
    for (index = 0u; index < capacity; ++index) {
        if (value[index] == 0u) return 1;
    }
    return 0;
}

static inline int __rin_account_errno(intptr_t result)
{
    uintptr_t error;
    if (result == 0) return 0;
    if (result > 0) return EIO;
    error = (uintptr_t)0 - (uintptr_t)result;
    return error > 4095u ? EIO : (int)error;
}

static inline void __rin_account_zero(void* value, size_t size)
{
    uint8_t* bytes = (uint8_t*)value;
    size_t index;
    for (index = 0u; index < size; ++index) bytes[index] = 0u;
}

static inline int __rin_credentials_get(__rin_credentials_v1* credentials)
{
    uint32_t left;
    uint32_t right;
    int error;
    if (!credentials) return EFAULT;
    __rin_account_zero(credentials, sizeof(*credentials));
    credentials->struct_size = sizeof(*credentials);
    credentials->version = __RIN_CREDENTIALS_VERSION_1;
    error = __rin_account_errno(_RIN_CREDENTIAL_SYSCALL1(
        RIN_SYS_CREDENTIALS_GET, credentials));
    if (error != 0) return error;
    if (credentials->struct_size != sizeof(*credentials) ||
        credentials->version != __RIN_CREDENTIALS_VERSION_1 ||
        credentials->group_count > __RIN_CREDENTIALS_MAX_GROUPS ||
        credentials->personality > 3u || credentials->flags != 0u) {
        return EIO;
    }
    for (left = 0u; left < credentials->group_count; ++left) {
        for (right = left + 1u; right < credentials->group_count; ++right) {
            if (credentials->groups[left] == credentials->groups[right])
                return EIO;
        }
    }
    for (left = credentials->group_count;
         left < __RIN_CREDENTIALS_MAX_GROUPS; ++left) {
        if (credentials->groups[left] != 0u) return EIO;
    }
    for (left = 0u; left < 4u; ++left) {
        if (credentials->reserved[left] != 0u) return EIO;
    }
    return 0;
}

static inline int __rin_account_validate_info(const __rin_account_info_v1* info)
{
    const uint16_t allowed_flags = __RIN_ACCOUNT_INFO_FLAG_ACTIVE |
        __RIN_ACCOUNT_INFO_FLAG_ADMIN;
    size_t index;
    if (!info || info->struct_size != sizeof(*info) ||
        info->version != __RIN_ACCOUNT_INFO_VERSION ||
        (info->flags & (uint16_t)~allowed_flags) != 0u ||
        info->reserved0 != 0u ||
        !__rin_account_string_terminated(info->name, sizeof(info->name)) ||
        !__rin_account_string_terminated(info->display_name,
                                         sizeof(info->display_name)) ||
        !__rin_account_string_terminated(info->home_dir,
                                         sizeof(info->home_dir)) ||
        !__rin_account_string_terminated(info->shell,
                                         sizeof(info->shell))) {
        return EIO;
    }
    for (index = 0u; index < 4u; ++index) {
        if (info->reserved[index] != 0u) return EIO;
    }
    return 0;
}

static inline int __rin_account_query(uint16_t kind, uint32_t uid,
                                      const char* name,
                                      __rin_account_info_v1* info)
{
    __rin_account_query_v1 query;
    size_t index = 0u;
    intptr_t result;
    int error;
    if (!info) return EFAULT;
    __rin_account_zero(&query, sizeof(query));
    query.struct_size = sizeof(query);
    query.version = __RIN_ACCOUNT_QUERY_VERSION;
    query.kind = kind;
    query.uid = uid;
    if (kind == __RIN_ACCOUNT_QUERY_BY_NAME) {
        if (!name || name[0] == '\0') return EINVAL;
        while (index < sizeof(query.name) && name[index] != '\0') {
            query.name[index] = (uint8_t)name[index];
            ++index;
        }
        if (index == sizeof(query.name)) return EINVAL;
    } else if ((kind != __RIN_ACCOUNT_QUERY_BY_UID &&
                kind != __RIN_ACCOUNT_QUERY_BY_INDEX) || name != 0) {
        return EINVAL;
    }

    __rin_account_zero(info, sizeof(*info));
    info->struct_size = sizeof(*info);
    info->version = __RIN_ACCOUNT_INFO_VERSION;
    result = _RIN_ACCOUNT_SYSCALL2(RIN_SYS_ACCOUNT_QUERY, &query, info);
    error = __rin_account_errno(result);
    if (error != 0) return error;
    return __rin_account_validate_info(info);
}

static inline size_t __rin_account_string_size(const uint8_t* value,
                                               size_t capacity)
{
    size_t size = 0u;
    while (size < capacity && value[size] != 0u) ++size;
    return size < capacity ? size + 1u : 0u;
}

static inline void __rin_account_copy(char* destination,
                                      const uint8_t* source,
                                      size_t size)
{
    size_t index;
    for (index = 0u; index < size; ++index) {
        destination[index] = (char)source[index];
    }
}

#endif /* RIN_LIBC_ACCOUNT_COMPAT_H */
