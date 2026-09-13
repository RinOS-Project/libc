/* SPDX-License-Identifier: MIT */
/*
 * RinOS libc - sys/types.h
 * POSIX基本型定義
 */

#ifndef _SYS_TYPES_H
#define _SYS_TYPES_H

#include "../stddef.h"
#include "../stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Hosted MinGW pthread/process headers include the platform sys/types owner
 * first.  Re-declaring its ABI-sized typedefs (notably pid_t, ino_t, mode_t,
 * and pthread_once_t) would make a harmless transitive include fail. */
#if defined(_INC_TYPES) && defined(__STDC_HOSTED__) && __STDC_HOSTED__
#define RIN_SYS_TYPES_HOST_OWNER 1
#endif

/* ═══════════════════════════════════════════════════════════════
 * 基本型定義
 * ═══════════════════════════════════════════════════════════════*/

#if !defined(RIN_SYS_TYPES_HOST_OWNER)

/* プロセス/ユーザー関連 */
typedef int            pid_t;      /* プロセスID */
typedef int            uid_t;      /* ユーザーID */
typedef int            gid_t;      /* グループID */
typedef unsigned int   id_t;       /* 汎用ID */

/* ファイル関連 */
typedef long           off_t;      /* ファイルオフセット */
typedef long long      off64_t;    /* 64ビットオフセット */
typedef unsigned long  ino_t;      /* iノード番号 */
typedef unsigned long  ino64_t;    /* 64ビットiノード */
typedef unsigned int   dev_t;      /* デバイス番号 */
typedef unsigned int   nlink_t;    /* リンクカウント */
typedef unsigned int   mode_t;     /* ファイルモード */
typedef long           blksize_t;  /* ブロックサイズ */
typedef long           blkcnt_t;   /* ブロック数 */
typedef long long      blkcnt64_t; /* 64ビットブロック数 */

/* サイズ関連。Hosted MinGW headers may already own ssize_t; do not redeclare
 * it with the target's long-width carrier when they were included first. */
#ifndef _SSIZE_T_DEFINED
typedef long           ssize_t;    /* 符号付きサイズ */
#define _SSIZE_T_DEFINED
#endif

/* 時間関連 */
#ifndef _TIME_T_DEFINED
typedef long           time_t;     /* 時刻 */
#define _TIME_T_DEFINED
#endif
#ifndef _CLOCK_T_DEFINED
typedef long           clock_t;    /* クロック刻み */
#define _CLOCK_T_DEFINED
#endif
#ifndef _CLOCKID_T_DEFINED
typedef int            clockid_t;  /* クロックID */
#define _CLOCKID_T_DEFINED
#endif
typedef unsigned int   useconds_t; /* マイクロ秒 */
typedef long           suseconds_t;/* 符号付きマイクロ秒 */

/* キー関連 */
typedef int            key_t;      /* IPC キー */

/* ═══════════════════════════════════════════════════════════════
 * デバイス番号マクロ
 * ═══════════════════════════════════════════════════════════════*/

#define makedev(maj, min)  ((dev_t)(((maj) << 8) | (min)))
#define major(dev)         ((unsigned int)(((dev) >> 8) & 0xFF))
#define minor(dev)         ((unsigned int)((dev) & 0xFF))

/* pthread関連型はpthread.hで定義 - 循環参照を避けるため基本型のみここで定義 */
#ifndef _PTHREAD_H
typedef uintptr_t      pthread_t;
typedef unsigned int   pthread_key_t;
typedef int            pthread_once_t;
#endif

#else
/* MinGW's sys/types.h intentionally omits several POSIX bookkeeping types
 * that Rin's fcntl/stat declarations still expose.  Add only those absent
 * aliases; never replace the host's width-sensitive pid/off/mode/ino types. */
typedef unsigned int uid_t;
typedef unsigned int gid_t;
typedef unsigned int id_t;
typedef unsigned int nlink_t;
typedef long blksize_t;
typedef long blkcnt_t;
typedef long long blkcnt64_t;
typedef unsigned long ino64_t;
typedef long suseconds_t;
typedef int key_t;
#define RIN_SYS_TYPES_UNSIGNED_ID 1
#ifndef FD_SETSIZE
#define FD_SETSIZE 1024
#endif
#endif /* !RIN_SYS_TYPES_HOST_OWNER */

/* ═══════════════════════════════════════════════════════════════
 * fd_set (select用)
 * ═══════════════════════════════════════════════════════════════*/

#if !defined(RIN_SYS_TYPES_HOST_OWNER)
#define FD_SETSIZE 1024

typedef struct {
    unsigned long fds_bits[FD_SETSIZE / (8 * sizeof(unsigned long))];
} fd_set;

#define FD_ZERO(set)     do { \
    for (unsigned int _i = 0; _i < sizeof((set)->fds_bits)/sizeof((set)->fds_bits[0]); _i++) \
        (set)->fds_bits[_i] = 0; \
} while(0)

#define FD_SET(fd, set)   ((set)->fds_bits[(fd) / (8 * sizeof(unsigned long))] |= \
                          (1UL << ((fd) % (8 * sizeof(unsigned long)))))

#define FD_CLR(fd, set)   ((set)->fds_bits[(fd) / (8 * sizeof(unsigned long))] &= \
                          ~(1UL << ((fd) % (8 * sizeof(unsigned long)))))

#define FD_ISSET(fd, set) ((set)->fds_bits[(fd) / (8 * sizeof(unsigned long))] & \
                          (1UL << ((fd) % (8 * sizeof(unsigned long)))))
#endif /* !RIN_SYS_TYPES_HOST_OWNER */

#ifdef __cplusplus
}
#endif

#undef RIN_SYS_TYPES_HOST_OWNER

#endif /* _SYS_TYPES_H */
