/*
 * RinOS libc - sys/un.h
 * UNIXドメインソケット
 */

#ifndef _SYS_UN_H
#define _SYS_UN_H

#include "types.h"
#include "socket.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════
 * UNIXドメインソケットアドレス
 * (sys/socket.hでも定義済みだが、互換性のため再定義)
 * ═══════════════════════════════════════════════════════════════*/

#ifndef _SOCKADDR_UN_DEFINED
#define _SOCKADDR_UN_DEFINED

#define UNIX_PATH_MAX 108

struct sockaddr_un {
    sa_family_t sun_family;         /* AF_UNIX */
    char        sun_path[UNIX_PATH_MAX]; /* パス名 */
};

#endif /* _SOCKADDR_UN_DEFINED */

/* ═══════════════════════════════════════════════════════════════
 * アドレス長マクロ
 * ═══════════════════════════════════════════════════════════════*/

/* sun_pathの実際の長さからsockaddr_unの長さを計算 */
#define SUN_LEN(ptr) \
    ((size_t)(((struct sockaddr_un*)0)->sun_path) + \
     _sun_strlen((ptr)->sun_path))

static inline size_t _sun_strlen(const char* s) {
    size_t len = 0;
    while (s[len]) len++;
    return len;
}

#ifdef __cplusplus
}
#endif

#endif /* _SYS_UN_H */
