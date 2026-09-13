/*
 * RinOS libc - grp.h
 * グループデータベース
 * Backed by the versioned Rin account-query ABI. RinOS currently exposes one
 * primary private group per account.
 */

#ifndef _GRP_H
#define _GRP_H

#include "stddef.h"
#include "sys/types.h"
#include "errno.h"
#include "rin_account_compat.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════
 * group構造体
 * ═══════════════════════════════════════════════════════════════*/

struct group {
    char*  gr_name;    /* グループ名 */
    char*  gr_passwd;  /* グループパスワード */
    gid_t  gr_gid;     /* グループID */
    char** gr_mem;     /* メンバー一覧 */
};

/* Non-reentrant group queries and enumeration expose pointers into private
 * scratch.  Keep the POSIX lifetime (until this thread's next call) without
 * allowing a concurrent account lookup to overwrite another thread's result.
 */
#if defined(__cplusplus)
#define RIN_GROUP_THREAD_LOCAL thread_local
#else
#define RIN_GROUP_THREAD_LOCAL _Thread_local
#endif

typedef struct __rin_group_runtime_state_v1 {
    struct group entry;
    union {
        char bytes[128];
        void* align;
    } storage;
    uint32_t cursor;
} __rin_group_runtime_state_v1;

extern RIN_GROUP_THREAD_LOCAL __rin_group_runtime_state_v1
    __rin_group_runtime_state;

/* ═══════════════════════════════════════════════════════════════
 * グループデータベース関数
 * ═══════════════════════════════════════════════════════════════*/

static inline int __rin_group_pack(const __rin_account_info_v1* info,
                                   struct group* grp,
                                   char* buf, size_t buflen,
                                   struct group** result) {
    const uint8_t encrypted[] = {'x', 0};
    size_t name_size;
    size_t offset = 0u;
    size_t aligned;
    uintptr_t aligned_address;
    char** members;
    if (result) *result = NULL;
    if (!info || !grp || !buf || !result) return EINVAL;
    name_size = __rin_account_string_size(info->name, sizeof(info->name));
    if (name_size == 0u || name_size > buflen) return ERANGE;
    grp->gr_name = buf;
    __rin_account_copy(buf, info->name, name_size);
    offset = name_size;
    if (sizeof(encrypted) > buflen - offset) return ERANGE;
    grp->gr_passwd = buf + offset;
    __rin_account_copy(buf + offset, encrypted, sizeof(encrypted));
    offset += sizeof(encrypted);
    aligned_address = ((uintptr_t)(void*)(buf + offset) + sizeof(char*) - 1u) &
        ~((uintptr_t)sizeof(char*) - 1u);
    if (aligned_address < (uintptr_t)(void*)buf) return ERANGE;
    aligned = (size_t)(aligned_address - (uintptr_t)(void*)buf);
    if (aligned < offset || aligned > buflen ||
        sizeof(char*) * 2u > buflen - aligned) return ERANGE;
    members = (char**)(void*)(buf + aligned);
    members[0] = grp->gr_name;
    members[1] = NULL;
    grp->gr_gid = (gid_t)info->gid;
    grp->gr_mem = members;
    *result = grp;
    return 0;
}

/* getgrnam_r - スレッドセーフ版getgrnam */
static inline int getgrnam_r(const char* name, struct group* grp,
                             char* buf, size_t buflen,
                             struct group** result) {
    __rin_account_info_v1 info;
    int error;
    if (result) *result = NULL;
    if (!name || !grp || !buf || !result) return EINVAL;
    error = __rin_account_query(__RIN_ACCOUNT_QUERY_BY_NAME, 0u, name, &info);
    if (error != 0) return error;
    return __rin_group_pack(&info, grp, buf, buflen, result);
}

/* getgrgid_r - スレッドセーフ版getgrgid */
static inline int getgrgid_r(gid_t gid, struct group* grp,
                             char* buf, size_t buflen,
                             struct group** result) {
    __rin_account_info_v1 info;
    int error;
    if (result) *result = NULL;
    if (!grp || !buf || !result || gid < 0) return EINVAL;
    error = __rin_account_query(__RIN_ACCOUNT_QUERY_BY_UID, (uint32_t)gid,
                                NULL, &info);
    if (error != 0) return error;
    return __rin_group_pack(&info, grp, buf, buflen, result);
}

/* getgrnam - グループ名でエントリを検索 */
static inline struct group* getgrnam(const char* name) {
    struct group* result = NULL;
    int error = getgrnam_r(name, &__rin_group_runtime_state.entry,
                           __rin_group_runtime_state.storage.bytes,
                           sizeof(__rin_group_runtime_state.storage.bytes),
                           &result);
    if (error != 0) {
        errno = error;
        return NULL;
    }
    return result;
}

/* getgrgid - GIDでエントリを検索 */
static inline struct group* getgrgid(gid_t gid) {
    struct group* result = NULL;
    int error = getgrgid_r(gid, &__rin_group_runtime_state.entry,
                           __rin_group_runtime_state.storage.bytes,
                           sizeof(__rin_group_runtime_state.storage.bytes),
                           &result);
    if (error != 0) {
        errno = error;
        return NULL;
    }
    return result;
}

/* ═══════════════════════════════════════════════════════════════
 * グループファイル列挙
 * ═══════════════════════════════════════════════════════════════*/

/* setgrent - グループファイルを先頭に巻き戻す */
static inline void setgrent(void) {
    __rin_group_runtime_state.cursor = 0u;
}

/* endgrent - グループファイルを閉じる */
static inline void endgrent(void) {
    __rin_group_runtime_state.cursor = 0u;
}

/* getgrent - 次のエントリを取得 */
static inline struct group* getgrent(void) {
    __rin_account_info_v1 info;
    struct group* result = NULL;
    int error = __rin_account_query(__RIN_ACCOUNT_QUERY_BY_INDEX,
                                    __rin_group_runtime_state.cursor,
                                    NULL, &info);
    if (error == ENOENT) {
        return NULL;
    }
    if (error == 0) {
        error = __rin_group_pack(&info, &__rin_group_runtime_state.entry,
                                 __rin_group_runtime_state.storage.bytes,
                                 sizeof(__rin_group_runtime_state.storage.bytes),
                                 &result);
    }
    if (error != 0) {
        errno = error;
        return NULL;
    }
    if (__rin_group_runtime_state.cursor == UINT32_MAX) {
        errno = EOVERFLOW;
        return NULL;
    }
    ++__rin_group_runtime_state.cursor;
    return result;
}

#undef RIN_GROUP_THREAD_LOCAL

/* ═══════════════════════════════════════════════════════════════
 * グループメンバーシップ
 * ═══════════════════════════════════════════════════════════════*/

/* getgroups - 補助グループIDを取得 */
static inline int getgroups(int size, gid_t list[]) {
    __rin_credentials_v1 credentials;
    uint32_t index;
    int error;
    if (size < 0) {
        errno = EINVAL;
        return -1;
    }
    error = __rin_credentials_get(&credentials);
    if (error != 0) {
        errno = error;
        return -1;
    }
    if (size == 0) return (int)credentials.group_count;
    if (!list) {
        errno = EFAULT;
        return -1;
    }
    if ((uint32_t)size < credentials.group_count) {
        errno = EINVAL;
        return -1;
    }
    for (index = 0u; index < credentials.group_count; ++index) {
        list[index] = (gid_t)credentials.groups[index];
    }
    return (int)credentials.group_count;
}

/* setgroups - 補助グループIDを設定 */
static inline int setgroups(size_t size, const gid_t* list) {
    __rin_credentials_v1 credentials;
    __rin_supplementary_groups_v1 request;
    size_t index;
    int error;
    if (size > __RIN_CREDENTIALS_MAX_GROUPS) {
        errno = EINVAL;
        return -1;
    }
    if (size != 0u && !list) {
        errno = EFAULT;
        return -1;
    }
    __rin_account_zero(&request, sizeof(request));
    error = __rin_credentials_get(&credentials);
    if (error != 0) {
        errno = error;
        return -1;
    }
    request.struct_size = sizeof(request);
    request.version = __RIN_SUPPLEMENTARY_GROUPS_VERSION;
    request.operation = __RIN_SUPPLEMENTARY_GROUPS_OPERATION_SET;
    request.target_uid = credentials.effective_uid;
    request.group_count = (uint32_t)size;
    for (index = 0u; index < size; ++index) {
        if (list[index] < 0) {
            errno = EINVAL;
            return -1;
        }
        request.groups[index] = (uint32_t)list[index];
    }
    error = __rin_account_errno(_RIN_CREDENTIAL_SYSCALL1(
        RIN_SYS_CREDENTIAL_GROUPS_SET, &request));
    if (error != 0) {
        errno = error;
        return -1;
    }
    return 0;
}

/* initgroups - グループアクセスリストを初期化 */
static inline int initgroups(const char* user, gid_t group) {
    __rin_account_info_v1 info;
    __rin_supplementary_groups_v1 request;
    int error;
    if (!user || user[0] == '\0' || group < 0) {
        errno = EINVAL;
        return -1;
    }
    __rin_account_zero(&request, sizeof(request));
    error = __rin_account_query(__RIN_ACCOUNT_QUERY_BY_NAME, 0u, user,
                                &info);
    if (error != 0) {
        errno = error;
        return -1;
    }
    if (info.gid != (uint32_t)group) {
        errno = EINVAL;
        return -1;
    }
    request.struct_size = sizeof(request);
    request.version = __RIN_SUPPLEMENTARY_GROUPS_VERSION;
    request.operation = __RIN_SUPPLEMENTARY_GROUPS_OPERATION_SET;
    request.target_uid = info.uid;
    request.group_count = 1u;
    request.groups[0] = (uint32_t)group;
    error = __rin_account_errno(_RIN_CREDENTIAL_SYSCALL1(
        RIN_SYS_CREDENTIAL_GROUPS_SET, &request));
    if (error != 0) {
        errno = error;
        return -1;
    }
    return 0;
}

#ifdef __cplusplus
}
#endif

#endif /* _GRP_H */
