/*
 * RinOS libc - pwd.h
 * パスワードデータベース
 * Backed by the versioned Rin account-query ABI.
 */

#ifndef _PWD_H
#define _PWD_H

#include "errno.h"
#include "stddef.h"
#include "sys/types.h"
#include "rin_account_compat.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════
 * passwd構造体
 * ═══════════════════════════════════════════════════════════════*/

struct passwd {
    char*  pw_name;    /* ユーザー名 */
    char*  pw_passwd;  /* 暗号化パスワード */
    uid_t  pw_uid;     /* ユーザーID */
    gid_t  pw_gid;     /* グループID */
    char*  pw_gecos;   /* ユーザー情報 */
    char*  pw_dir;     /* ホームディレクトリ */
    char*  pw_shell;   /* ログインシェル */
};

static inline int __rin_pwd_uid_negative(uid_t uid) {
    union {
        uid_t value;
        int signed_value;
    } bits;
    bits.value = uid;
    /* uid_t is a 32-bit signed target type or a 32-bit hosted unsigned type;
     * examining the same-width representation avoids a tautological
     * unsigned comparison while retaining the target's negative rejection. */
    return sizeof(uid_t) == sizeof(int) && bits.signed_value < 0;
}

/* Non-reentrant passwd queries return pointers into private scratch.  Keep
 * that scratch per thread so another query cannot overwrite a live result. */
#if defined(__cplusplus)
#define RIN_PWD_THREAD_LOCAL thread_local
#else
#define RIN_PWD_THREAD_LOCAL _Thread_local
#endif

/* ═══════════════════════════════════════════════════════════════
 * パスワードデータベース関数
 * ═══════════════════════════════════════════════════════════════*/

static inline int __rin_passwd_pack(const __rin_account_info_v1* info,
                                    struct passwd* pwd,
                                    char* buf, size_t buflen,
                                    struct passwd** result) {
    const uint8_t encrypted[] = {'x', 0};
    size_t name_size;
    size_t display_size;
    size_t home_size;
    size_t shell_size;
    size_t offset = 0u;
    if (result) *result = NULL;
    if (!info || !pwd || !buf || !result) return EINVAL;
    name_size = __rin_account_string_size(info->name, sizeof(info->name));
    display_size = __rin_account_string_size(info->display_name,
                                              sizeof(info->display_name));
    home_size = __rin_account_string_size(info->home_dir,
                                           sizeof(info->home_dir));
    shell_size = __rin_account_string_size(info->shell, sizeof(info->shell));
    if (name_size == 0u || display_size == 0u || home_size == 0u ||
        shell_size == 0u || name_size > buflen) return ERANGE;

    pwd->pw_name = buf + offset;
    __rin_account_copy(buf + offset, info->name, name_size);
    offset += name_size;
    if (sizeof(encrypted) > buflen - offset) return ERANGE;
    pwd->pw_passwd = buf + offset;
    __rin_account_copy(buf + offset, encrypted, sizeof(encrypted));
    offset += sizeof(encrypted);
    if (display_size > buflen - offset) return ERANGE;
    pwd->pw_gecos = buf + offset;
    __rin_account_copy(buf + offset, info->display_name, display_size);
    offset += display_size;
    if (home_size > buflen - offset) return ERANGE;
    pwd->pw_dir = buf + offset;
    __rin_account_copy(buf + offset, info->home_dir, home_size);
    offset += home_size;
    if (shell_size > buflen - offset) return ERANGE;
    pwd->pw_shell = buf + offset;
    __rin_account_copy(buf + offset, info->shell, shell_size);
    pwd->pw_uid = (uid_t)info->uid;
    pwd->pw_gid = (gid_t)info->gid;
    *result = pwd;
    return 0;
}

/* getpwnam_r - スレッドセーフ版getpwnam */
static inline int getpwnam_r(const char* name, struct passwd* pwd,
                             char* buf, size_t buflen,
                             struct passwd** result) {
    __rin_account_info_v1 info;
    int error;
    if (result) *result = NULL;
    if (!name || !pwd || !buf || !result) return EINVAL;
    error = __rin_account_query(__RIN_ACCOUNT_QUERY_BY_NAME, 0u, name, &info);
    if (error != 0) return error;
    return __rin_passwd_pack(&info, pwd, buf, buflen, result);
}

/* getpwuid_r - スレッドセーフ版getpwuid */
static inline int getpwuid_r(uid_t uid, struct passwd* pwd,
                             char* buf, size_t buflen,
                             struct passwd** result) {
    __rin_account_info_v1 info;
    int error;
    if (result) *result = NULL;
    if (!pwd || !buf || !result || __rin_pwd_uid_negative(uid)) return EINVAL;
    error = __rin_account_query(__RIN_ACCOUNT_QUERY_BY_UID, (uint32_t)uid,
                                NULL, &info);
    if (error != 0) return error;
    return __rin_passwd_pack(&info, pwd, buf, buflen, result);
}

/* getpwnam - ユーザー名でエントリを検索 */
static inline struct passwd* getpwnam(const char* name) {
    static RIN_PWD_THREAD_LOCAL struct passwd entry;
    static RIN_PWD_THREAD_LOCAL char buffer[512];
    struct passwd* result = NULL;
    int error = getpwnam_r(name, &entry, buffer, sizeof(buffer), &result);
    if (error != 0) {
        errno = error;
        return NULL;
    }
    return result;
}

/* getpwuid - UIDでエントリを検索 */
static inline struct passwd* getpwuid(uid_t uid) {
    static RIN_PWD_THREAD_LOCAL struct passwd entry;
    static RIN_PWD_THREAD_LOCAL char buffer[512];
    struct passwd* result = NULL;
    int error = getpwuid_r(uid, &entry, buffer, sizeof(buffer), &result);
    if (error != 0) {
        errno = error;
        return NULL;
    }
    return result;
}

/* ═══════════════════════════════════════════════════════════════
 * パスワードファイル列挙
 * ═══════════════════════════════════════════════════════════════*/

static RIN_PWD_THREAD_LOCAL uint32_t __rin_passwd_cursor;

/* setpwent - パスワードファイルを先頭に巻き戻す */
static inline void setpwent(void) {
    __rin_passwd_cursor = 0u;
}

/* endpwent - パスワードファイルを閉じる */
static inline void endpwent(void) {
    __rin_passwd_cursor = 0u;
}

/* getpwent - 次のエントリを取得 */
static inline struct passwd* getpwent(void) {
    static RIN_PWD_THREAD_LOCAL struct passwd entry;
    static RIN_PWD_THREAD_LOCAL char buffer[512];
    __rin_account_info_v1 info;
    struct passwd* result = NULL;
    int error = __rin_account_query(__RIN_ACCOUNT_QUERY_BY_INDEX,
                                    __rin_passwd_cursor, NULL, &info);
    if (error == ENOENT) {
        return NULL;
    }
    if (error == 0) {
        error = __rin_passwd_pack(&info, &entry, buffer, sizeof(buffer),
                                  &result);
    }
    if (error != 0) {
        errno = error;
        return NULL;
    }
    if (__rin_passwd_cursor == UINT32_MAX) {
        errno = EOVERFLOW;
        return NULL;
    }
    ++__rin_passwd_cursor;
    return result;
}

#undef RIN_PWD_THREAD_LOCAL

#ifdef __cplusplus
}
#endif

#endif /* _PWD_H */
